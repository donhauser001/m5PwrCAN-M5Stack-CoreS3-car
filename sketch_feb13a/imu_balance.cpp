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

static unsigned long lastDebugMs   = 0;
static float         filteredGyro  = 0;

// ---- 稳定计数 (diagMode 下统计连续满足启动条件的控制周期数) ----
static int           stableCount   = 0;
static int           fallConfirmCount = 0;

// ---- 软启动状态 ----
static bool          softStartActive = false;
static unsigned long softStartMs     = 0;

// ---- 架空轮阶跃实验状态 ----
static unsigned long benchLastStepMs = 0;
static int           benchCmdRpm     = 0;
static int           benchStartRpm   = -1;
static int           lastCmdRpmR     = 0;
static int           lastCmdRpmL     = 0;

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
            Serial.println("[BENCH] sweep complete. stopping test.");
            if (benchStartRpm > 0) {
                Serial.printf("[BENCH] first moving step ~= %d RPM\n", benchStartRpm);
            } else {
                Serial.println("[BENCH] no motion detected up to 30 RPM");
            }
            stopBenchStepInternal();
            return;
        }
        Serial.printf("[BENCH] step cmd=%d RPM\n", benchCmdRpm);
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
        Serial.printf("[BENCH] motion threshold detected at cmd=%d RPM (act=%.1f RPM)\n",
                      benchStartRpm, rpmAbs);
    }
}

// ============ IMU 校准 ============
void calibrateIMU() {
    Serial.println("Calibrating IMU (6-axis)... hold upright!");
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.setCursor(10, screenH / 2 - 10);
    M5.Lcd.println("Calibrating...");
    M5.Lcd.println("Hold at balance!");

    float gxSum = 0, gySum = 0, gzSum = 0;
    float axSum = 0, aySum = 0, azSum = 0;
    int cnt = 0;

    for (int i = 0; i < 500; i++) {
        M5.Imu.update();
        auto d = M5.Imu.getImuData();
        gxSum += d.gyro.x;
        gySum += d.gyro.y;
        gzSum += d.gyro.z;
        axSum += d.accel.x;
        aySum += d.accel.y;
        azSum += d.accel.z;
        cnt++;
        delay(4);
    }

    // 3轴陀螺仪零偏
    gyroOffset[0] = gxSum / cnt;
    gyroOffset[1] = gySum / cnt;
    gyroOffset[2] = gzSum / cnt;

    // 加速计计算平衡点倾角
    float avgAy = aySum / cnt;
    float avgAz = azSum / cnt;
    pitchOffset = atan2f(avgAz, avgAy) * RAD_TO_DEG;

    // 重置状态
    currentPitch    = 0;
    filteredGyro    = 0;
    pidIntegral     = 0;
    lastError       = 0;
    fallen          = false;
    targetAngleFilt = 0;
    stableCount     = 0;
    fallConfirmCount = 0;
    softStartActive = false;

    Serial.println("======== Calibration Result ========");
    Serial.printf("gyroOff: [%.2f, %.2f, %.2f] dps\n",
                  gyroOffset[0], gyroOffset[1], gyroOffset[2]);
    Serial.printf("pitchOff: %.1f deg\n", pitchOffset);
    Serial.printf("accel avg: [%.3f, %.3f, %.3f] g\n",
                  axSum / cnt, avgAy, avgAz);
    Serial.printf("BALANCE_DIR: %d\n", BALANCE_DIR);
    Serial.println("====================================");
}

// ============ 姿态更新 (互补滤波) ============
void updateIMU(float dt) {
    M5.Imu.update();
    auto d = M5.Imu.getImuData();

    // 加速计倾角 (减去校准偏移)
    float accelAngle = atan2f(d.accel.z, d.accel.y) * RAD_TO_DEG - pitchOffset;

    // 陀螺仪角速度 (减去零偏), pitch 轴 = gyro.x
    // 注意: BMI270 在 CoreS3 上 gyro.x 符号与 atan2(az,ay) 定义的 pitch 方向相反,
    //       必须取反才能让互补滤波和 D 项正确工作!
    float rawGyroX = -(d.gyro.x - gyroOffset[0]);
    float rawGyroY = d.gyro.y - gyroOffset[1];
    float rawGyroZ = d.gyro.z - gyroOffset[2];

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

    // --- 诊断模式: 等待用户手扶静止后按 Stand ---
    if (diagMode) {
        clearControlOutputState();
        bool angleOk = fabs(controlPitch) < START_ANGLE;
        bool gyroOk  = fabs(gyroRate) < GYRO_START_THRESHOLD;
        if (angleOk && gyroOk) {
            stableCount++;
        } else {
            stableCount = 0;
        }

        unsigned long now = millis();
        if (now - lastDebugMs > 150) {
            lastDebugMs = now;
            Serial.printf("[DIAG] pitch=%+6.1f  gyro=%+7.1f  stable=%d/%d",
                          controlPitch, gyroRate, stableCount, STABLE_HOLD_COUNT);
            if (stableCount >= STABLE_HOLD_COUNT) {
                Serial.print("  ** READY **");
            } else if (angleOk) {
                Serial.print("  (hold still)");
            }
            Serial.println();
        }
        stopMotors();
        return;
    }

    if (fallen) {
        clearControlOutputState();
        stopMotors();
        return;
    }

    if (fabs(controlPitch) > FALL_ANGLE) {
        fallConfirmCount++;
        if (fallConfirmCount >= FALL_CONFIRM_COUNT) {
            stopMotors();
            fallen           = true;
            softStartActive  = false;
            stableCount      = 0;
            fallConfirmCount = 0;
            pidIntegral      = 0;
            clearControlOutputState();
            Serial.printf("FALLEN! pitch=%.1f\n", controlPitch);
            return;
        }
    } else {
        fallConfirmCount = 0;
    }

    // 目标角低通: 保留响应同时抑制突变
    targetAngleFilt = TARGET_LPF_ALPHA * targetAngleFilt
                    + (1.0f - TARGET_LPF_ALPHA) * targetAngle;

    // ---- 角度 PID (内环: 输出 RPM) ----
    float error = controlPitch - targetAngleFilt;
    pidIntegral += error * dt;
    pidIntegral = constrain(pidIntegral, -INTEGRAL_LIMIT, INTEGRAL_LIMIT);

    float pTerm = Kp * error;
    float iTerm = Ki * pidIntegral;
    float dTerm = constrain(Kd * filteredGyro, -D_LIMIT, D_LIMIT);

    float rawOutput = (pTerm + iTerm + dTerm) * BALANCE_DIR;
    rawOutput -= linearSpeed * VELOCITY_K;
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
        static unsigned long lastTempWarnMs = 0;
        if (millis() - lastTempWarnMs > 2000) {
            lastTempWarnMs = millis();
            Serial.printf("TEMP THROTTLE! max=%.0f°C throttle=%.0f%%\n", tempMax, throttle * 100);
        }
    }

    // ---- 转向 (手机 X 输入) ----
    float steer = phoneX * 5.0f;
    int baseOut = constrain((int)pidOutput, -OUTPUT_LIMIT, OUTPUT_LIMIT);
    dbgAfterDeadzone = (float)baseOut;

    int targetR = constrain((int)(pidOutput + steer), -OUTPUT_LIMIT, OUTPUT_LIMIT);
    int targetL = constrain((int)(pidOutput - steer), -OUTPUT_LIMIT, OUTPUT_LIMIT);
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

    // 串口调试 (每100ms)
    unsigned long now = millis();
    if (now - lastDebugMs > 100) {
        lastDebugMs = now;
        Serial.printf("dt=%5.1fms raw=%+6.1f clamp=%+6.1f dz=%+5.0f sent=%+4d/%+4d act=%+4d/%+4d cur=%+5.0f/%+5.0f pitch=%+5.2f gyro=%+7.1f\n",
                      (double)ctrlDtMs, (double)dbgPidRaw, (double)dbgPidClamped, (double)dbgAfterDeadzone,
                      dbgSentR, dbgSentL, actualSpdR, actualSpdL, (double)actualCurrentR, (double)actualCurrentL,
                      (double)controlPitch, (double)gyroRate);
    }
}

// ============ 安全启动: 角度 + 角速度 + 连续稳定窗口三重门限 ============
bool activateBalance() {
    if (benchMode) {
        Serial.println(">>> BENCH MODE active, stop bench first (B,0) <<<");
        return false;
    }

    float controlPitch = currentPitch + PITCH_MOUNT_OFFSET;
    bool angleOk  = fabs(controlPitch) < START_ANGLE;
    bool gyroOk   = fabs(gyroRate)     < GYRO_START_THRESHOLD;
    bool stableOk = (stableCount >= STABLE_HOLD_COUNT);

    if (angleOk && gyroOk && stableOk) {
        diagMode        = false;
        fallen          = false;
        phoneX          = 0;
        phoneY          = 0;
        pidIntegral     = 0;
        lastError       = 0;
        fallConfirmCount = 0;
        targetAngle     = 0;
        targetAngleFilt = 0;
        softStartActive = true;
        softStartMs     = millis();
        stableCount     = 0;
        clearControlOutputState();
        Serial.printf(">>> BALANCE ON  pitch=%.1f° gyro=%.1f°/s  soft-start %dms <<<\n",
                      controlPitch, gyroRate, SOFT_START_MS);
        return true;
    }

    // 条件不满足: 打印原因
    Serial.printf(">>> NOT READY:  pitch=%.1f°(<%.0f) %s  gyro=%.1f°/s(<%.0f) %s  stable=%d/%d %s\n",
                  controlPitch, START_ANGLE,      angleOk  ? "OK" : "FAIL",
                  fabs(gyroRate), GYRO_START_THRESHOLD, gyroOk   ? "OK" : "FAIL",
                  stableCount, STABLE_HOLD_COUNT,       stableOk ? "OK" : "FAIL");
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
        Serial.println("[BENCH] step test ON. Lift wheels before running.");
        Serial.println("[BENCH] command sweep: 0..30 RPM, step every 700ms.");
        driveMotors(0, 0);
        return;
    }

    Serial.println("[BENCH] step test OFF.");
    stopBenchStepInternal();
}
