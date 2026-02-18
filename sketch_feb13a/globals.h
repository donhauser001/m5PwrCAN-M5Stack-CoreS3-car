#pragma once
/**
 * globals.h — 跨模块共享的状态变量 (extern 声明)
 */

#include <Arduino.h>

// ============ PID 参数 ============
extern float Kp, Ki, Kd;
extern float pidIntegral;
extern float lastError;
extern float pidOutput;

// ============ 控制链路调试 ============
extern float ctrlDtMs;           // 本控制周期时长(ms)
extern float dbgPidRaw;          // PID原始输出(未限幅)
extern float dbgPidClamped;      // PID限幅后
extern float dbgAfterDeadzone;   // 死区/静摩擦补偿后(基准输出)
extern int   dbgSentR, dbgSentL; // 实际发给电机的命令
extern bool  benchMode;          // 架空轮阶跃测试模式

// ============ IMU / 姿态 (6轴互补滤波) ============
extern float currentPitch;    // 前后倾角 (平衡主轴)
extern float currentRoll;     // 左右倾角 (辅助观察)
extern float currentYaw;      // 航向角 (积分累计, 会漂移)
extern float gyroRate;        // pitch 轴角速度 (PID 微分项)
extern float targetAngle;     // 目标倾角 (手机控制)
extern float targetAngleFilt; // 控制实际使用的目标倾角 (低通后)

// 调试: 原始加速度 Pitch (atan2(az,ay) 度)
extern float rawAccelPitchDeg;
extern float rawAccelAy;      // 原始 ay (g)
extern float rawAccelAz;      // 原始 az (g)

// ============ 控制状态 ============
extern bool fallen;
extern bool diagMode;          // 诊断模式 (只打印不驱动)
extern int  stableCount;       // 连续稳定计数 (显示用)

// ============ 手机输入 ============
extern float phoneX;
extern float phoneY;

// ============ 电机反馈 (指令 + 0x02 帧解析) ============
extern int cmdSpdR, cmdSpdL;           // 指令 RPM (速度模式)
extern float vinR, vinL;
extern int actualSpdR, actualSpdL;     // 实际 RPM (int, 兼容显示)
extern float actualSpeedR, actualSpeedL;   // 实际转速 RPM
extern float actualCurrentR, actualCurrentL; // 实际电流 mA
extern float actualPosR, actualPosL;       // 实际位置 °
extern int32_t encoderR, encoderL;         // 编码器累计值
extern float motorTempR, motorTempL;       // 电机温度 °C
extern float linearSpeed;                  // 线速度 mm/s (双轮平均)
extern float distanceMM;                  // 累计行驶距离 mm

// ============ CAN 诊断 ============
extern uint32_t canTxFailCount;  // CAN 发送失败计数 (控制环内 setMotorCurrent 丢帧)

// ============ 显示 ============
extern int screenW, screenH;
extern String myIP;
