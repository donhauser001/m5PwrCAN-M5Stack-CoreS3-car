/**
 * imu_balance.cpp — 6轴互补滤波姿态估算 + PID 自平衡控制
 *
 * CoreS3 传感器坐标系 (竖直放置时):
 *   Accel Y ≈ +1g (重力方向)
 *   前后倾斜 → Accel Z 变化
 *   pitch = atan2(accel.z, accel.y)
 *   pitch 角速度 = gyro.x
 */

#include "imu_balance.h"
#include "config.h"
#include "globals.h"
#include "can_motor.h"
#include <M5Unified.h>

static float         filteredGyro  = 0;
static float         filteredLinSpeed = 0;

// ---- 位置锁定: 记录"锚点", 漂移后自动回正 ----
static float         anchorDistanceMM = 0;
static bool          positionLockActive = false;

// ---- 稳定计数 (diagMode 下统计连续满足启动条件的控制周期数, stableCount 已在 globals) ----
static int           fallConfirmCount = 0;

// ---- 软启动状态 ----
static bool          softStartActive = false;
static unsigned long softStartMs     = 0;

// ---- 启动保护期: 从极限角度启动时暂时抑制跌倒检测 ----
static bool          startupGraceActive = false;
static unsigned long startupGraceMs     = 0;

// ---- 架空轮阶跃实验状态 ----
static unsigned long benchLastStepMs = 0;
static int           benchCmdRpm     = 0;
static int           benchStartRpm   = -1;
static int           lastCmdRpmR     = 0;
static int           lastCmdRpmL     = 0;

// ---- 移动控制输入平滑 ----
static float smoothPhoneX = 0;
static float smoothPhoneY = 0;

static void clearControlOutputState() {
    pidOutput = 0;
    cmdSpdR = 0;
    cmdSpdL = 0;
    dbgPidRaw = 0;
    dbgPidClamped = 0;
    dbgAfterDeadzone = 0;
    dbgSentR = 0;
    dbgSentL = 0;
    lastCmdRpmR = 0;
    lastCmdRpmL = 0;
}

static int applyOutputShaping(int rpm) {
    int mag = abs(rpm);
    if (OUTPUT_DEADBAND_RPM > 0 && mag <= OUTPUT_DEADBAND_RPM) return 0;
    if (MIN_EFFECTIVE_RPM > 0 && mag > 0 && mag < MIN_EFFECTIVE_RPM) mag = MIN_EFFECTIVE_RPM;
    return rpm > 0 ? mag : -mag;
}

static int applySlewLimit(int target, int previous) {
    if (OUTPUT_SLEW_RPM_PER_CYCLE <= 0) return target;
    int delta = constrain(target - previous, -OUTPUT_SLEW_RPM_PER_CYCLE, OUTPUT_SLEW_RPM_PER_CYCLE);
    return previous + delta;
}

static void stopBenchStepInternal() {
    benchMode = false;
    benchCmdRpm = 0;
    benchStartRpm = -1;
    clearControlOutputState();
    stopMotors();
}

static void runBenchStepTest() {
    unsigned long now = millis();

    // 每 700ms 增加一级命令: 0,1,2,...,30 RPM
    if (now - benchLastStepMs >= 700) {
        benchLastStepMs = now;
        benchCmdRpm++;
        if (benchCmdRpm > 30) {
            stopBenchStepInternal();
            return;
        }
    }

    cmdSpdR = benchCmdRpm;
    cmdSpdL = benchCmdRpm;
    pidOutput = (float)benchCmdRpm;
    dbgPidRaw = (float)benchCmdRpm;
    dbgPidClamped = (float)benchCmdRpm;
    dbgAfterDeadzone = (float)benchCmdRpm;
    dbgSentR = benchCmdRpm;
    dbgSentL = benchCmdRpm;
    driveMotors(benchCmdRpm, benchCmdRpm);

    float rpmAbs = (fabs(actualSpeedR) + fabs(actualSpeedL)) * 0.5f;
    if (benchStartRpm < 0 && benchCmdRpm > 0 && rpmAbs > 3.0f) {
        benchStartRpm = benchCmdRpm;
    }
}

// ============ 姿态更新 (互补滤波, 无需校准) ============
void updateIMU(float dt) {
    M5.Imu.update();
    auto d = M5.Imu.getImuData();

    // 原始加速度 Pitch (调试/屏幕显示)
    rawAccelPitchDeg = atan2f(d.accel.z, d.accel.y) * RAD_TO_DEG;
    rawAccelAy = d.accel.y;
    rawAccelAz = d.accel.z;
    // 加速计倾角 = 原始角度 (不做校准偏移, 由 PITCH_MOUNT_OFFSET 在 controlPitch 里补偿)
    float accelAngle = rawAccelPitchDeg;

    // 陀螺仪角速度 (直接使用, 不减零偏 — BMI270 静态零偏极小)
    // 注意: gyro.x 符号与 atan2(az,ay) 定义的 pitch 方向相反, 必须取反
    float rawGyroX = -d.gyro.x;
    float rawGyroY = d.gyro.y;
    float rawGyroZ = d.gyro.z;

    // 互补滤波 → pitch 角度 (前后)
    currentPitch = COMP_ALPHA * (currentPitch + rawGyroX * dt)
                 + (1.0f - COMP_ALPHA) * accelAngle;
    
    // 互补滤波 → roll 角度 (左右)
    // CoreS3 安装方向: Roll 对应 accel.x / accel.z
    float accelRoll = atan2f(d.accel.x, d.accel.z) * RAD_TO_DEG;
    currentRoll = COMP_ALPHA * (currentRoll + rawGyroY * dt)
                + (1.0f - COMP_ALPHA) * accelRoll;

    // 纯积分 → yaw 角度 (航向)
    currentYaw += rawGyroZ * dt;

    // D 项使用轻度低通，抑制尖峰噪声
    filteredGyro = GYRO_LPF_ALPHA * filteredGyro + (1.0f - GYRO_LPF_ALPHA) * rawGyroX;
    
    gyroRate = rawGyroX;
}

// ============ PID 平衡控制 ============
void balanceControl(float dt) {
    float controlPitch = currentPitch + PITCH_MOUNT_OFFSET;
    ctrlDtMs = dt * 1000.0f;

    if (benchMode) {
        runBenchStepTest();
        return;
    }

    // --- 诊断模式: 静止在允许角度内即可 READY，按 Stand 直接自立 ---
    if (diagMode) {
        clearControlOutputState();
        bool angleOk = fabs(controlPitch) < STANDUP_MAX_ANGLE;
        bool gyroOk  = fabs(gyroRate) < GYRO_START_THRESHOLD;
        if (angleOk && gyroOk) {
            stableCount++;
        } else {
            stableCount = 0;
        }

        stopMotors();
        return;
    }

    if (fallen) {
        clearControlOutputState();
        stopMotors();
        return;
    }

    // ---- 恢复模式管理: 角度回到安全区 或 超时后结束 ----
    if (startupGraceActive) {
        if (fabs(controlPitch) < RECOVERY_EXIT_ANGLE) {
            startupGraceActive = false;
        } else if ((millis() - startupGraceMs) >= STANDUP_GRACE_MS) {
            startupGraceActive = false;
        }
    }

    if (!startupGraceActive && fabs(controlPitch) > FALL_ANGLE) {
        fallConfirmCount++;
        if (fallConfirmCount >= FALL_CONFIRM_COUNT) {
            stopMotors();
            fallen              = true;
            softStartActive     = false;
            startupGraceActive  = false;
            positionLockActive  = false;
            stableCount         = 0;
            fallConfirmCount    = 0;
            pidIntegral         = 0;
            smoothPhoneX        = 0;
            smoothPhoneY        = 0;
            clearControlOutputState();
            return;
        }
    } else {
        fallConfirmCount = 0;
    }

    // 目标角低通: 保留响应同时抑制突变
    targetAngleFilt = TARGET_LPF_ALPHA * targetAngleFilt
                    + (1.0f - TARGET_LPF_ALPHA) * targetAngle;

    // ---- 位置+速度环: 修正目标角度 (而非 PID 输出) ----
    float adjustedTarget = targetAngleFilt;
    if (!startupGraceActive) {
        filteredLinSpeed = VELOCITY_LPF_ALPHA * filteredLinSpeed
                         + (1.0f - VELOCITY_LPF_ALPHA) * linearSpeed;
        if (!positionLockActive) {
            anchorDistanceMM = distanceMM;
            positionLockActive = true;
        }
        if (fabs(phoneY) > 1.0f)
            anchorDistanceMM = distanceMM;

        float posCorr = (distanceMM - anchorDistanceMM) * POSITION_K;
        float velCorr = filteredLinSpeed * VELOCITY_K;
        float totalCorr = constrain(posCorr + velCorr,
                                    -POS_VEL_CORR_LIMIT, POS_VEL_CORR_LIMIT);
        adjustedTarget += totalCorr;
    } else {
        filteredLinSpeed = 0;
        positionLockActive = false;
    }

    // ---- 角度 PID (内环: 输出 RPM) ----
    float useKp = startupGraceActive ? RECOVERY_KP : Kp;
    float useKd = startupGraceActive ? RECOVERY_KD : Kd;

    float error = controlPitch - adjustedTarget;
    if (fabs(error) < INTEGRAL_DECAY_THRESHOLD) {
        pidIntegral += error * dt;
    } else {
        pidIntegral *= INTEGRAL_DECAY_RATE;
    }
    pidIntegral = constrain(pidIntegral, -INTEGRAL_LIMIT, INTEGRAL_LIMIT);

    float pTerm = useKp * error;
    float iTerm = Ki * pidIntegral;
    float dTerm = constrain(useKd * filteredGyro, -D_LIMIT, D_LIMIT);

    float rawOutput = (pTerm + iTerm + dTerm) * BALANCE_DIR;
    dbgPidRaw = rawOutput;
    float clampedOutput = constrain(rawOutput, -(float)OUTPUT_LIMIT, (float)OUTPUT_LIMIT);
    dbgPidClamped = clampedOutput;

    // ---- 软启动斜坡 ----
    float softGain = 1.0f;
    if (softStartActive) {
        softGain = (float)(millis() - softStartMs) / (float)SOFT_START_MS;
        if (softGain >= 1.0f) {
            softGain        = 1.0f;
            softStartActive = false;
        }
    }
    pidOutput = clampedOutput * softGain;

    // ---- 温度保护降额 ----
    float tempMax = max(motorTempR, motorTempL);
    if (tempMax > TEMP_THROTTLE_DEG) {
        // 超温 20°C 时降到 30%，线性插值
        float throttle = constrain(1.0f - (tempMax - TEMP_THROTTLE_DEG) / 20.0f, 0.3f, 1.0f);
        pidOutput *= throttle;
    }

    // ---- 偏航修正: 轮速和 = 旋转分量, 反馈抑制原地自旋 ----
    float yawRate = ((float)actualSpeedR + (float)actualSpeedL) * 0.5f;
    float yawCorr = yawRate * YAW_K;

    // ---- 移动输入平滑 + 转向 (手机 X 输入, 增益 0.15 → max +-15 RPM) ----
    smoothPhoneY = MOVE_INPUT_LPF * smoothPhoneY + (1.0f - MOVE_INPUT_LPF) * phoneY;
    smoothPhoneX = STEER_INPUT_LPF * smoothPhoneX + (1.0f - STEER_INPUT_LPF) * phoneX;
    float steer = smoothPhoneX * STEER_GAIN;
    int baseOut = constrain((int)pidOutput, -OUTPUT_LIMIT, OUTPUT_LIMIT);
    dbgAfterDeadzone = (float)baseOut;

    int targetR = constrain((int)(pidOutput + steer - yawCorr), -OUTPUT_LIMIT, OUTPUT_LIMIT);
    int targetL = constrain((int)(pidOutput - steer + yawCorr), -OUTPUT_LIMIT, OUTPUT_LIMIT);
    int slewR = applySlewLimit(targetR, lastCmdRpmR);
    int slewL = applySlewLimit(targetL, lastCmdRpmL);
    lastCmdRpmR = slewR;
    lastCmdRpmL = slewL;

    int outR = applyOutputShaping(slewR);
    int outL = applyOutputShaping(slewL);
    dbgSentR = outR;
    dbgSentL = outL;

    cmdSpdR = outR;
    cmdSpdL = outL;
    driveMotors(outR, outL);

    distanceMM += linearSpeed * dt;

}

// ============ 安全启动: 角度 + 角速度 + 连续稳定窗口三重门限 ============
bool activateBalance() {
    if (benchMode) {
        return false;
    }

    float controlPitch = currentPitch + PITCH_MOUNT_OFFSET;
    bool angleOk  = fabs(controlPitch) < STANDUP_MAX_ANGLE;
    bool gyroOk   = fabs(gyroRate)     < GYRO_START_THRESHOLD;
    bool stableOk = (stableCount >= STABLE_HOLD_COUNT);

    if (angleOk && gyroOk && stableOk) {
        diagMode        = false;
        fallen          = false;
        phoneX          = 0;
        phoneY          = 0;
        smoothPhoneX    = 0;
        smoothPhoneY    = 0;
        pidIntegral        = 0;
        lastError          = 0;
        filteredLinSpeed    = 0;
        fallConfirmCount   = 0;
        targetAngle        = 0;
        targetAngleFilt    = 0;
        stableCount        = 0;
        distanceMM         = 0;
        anchorDistanceMM   = 0;
        positionLockActive = false;
        clearControlOutputState();
        if (fabs(controlPitch) >= RECOVERY_ENTER_ANGLE) {
            // 大角度启动(>8°): 恢复模式, 跳过软启动, 立即全力回正
            startupGraceActive = true;
            startupGraceMs     = millis();
            softStartActive    = false;
        } else {
            // 小角度启动: 软启动斜坡
            startupGraceActive = false;
            softStartActive    = true;
            softStartMs        = millis();
        }
        return true;
    }

    return false;
}

void setBenchStepTest(bool on) {
    if (on) {
        benchMode = true;
        diagMode = true;
        fallen = false;
        pidIntegral = 0;
        targetAngle = 0;
        targetAngleFilt = 0;
        benchCmdRpm = 0;
        benchStartRpm = -1;
        benchLastStepMs = millis();
        pidOutput = 0;
        driveMotors(0, 0);
        return;
    }

    stopBenchStepInternal();
}
