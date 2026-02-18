/**
 * globals.cpp — 跨模块共享的状态变量 (定义)
 */

#include "globals.h"
#include "config.h"

// PID
float Kp = DEFAULT_KP;
float Ki = DEFAULT_KI;
float Kd = DEFAULT_KD;
float pidIntegral = 0;
float lastError   = 0;
float pidOutput   = 0;
float ctrlDtMs = 0;
float dbgPidRaw = 0;
float dbgPidClamped = 0;
float dbgAfterDeadzone = 0;
int   dbgSentR = 0, dbgSentL = 0;
bool  benchMode = false;

// IMU / 姿态 (6轴互补滤波)
float currentPitch = 0;
float currentRoll  = 0;
float currentYaw   = 0;
float gyroRate     = 0;
float targetAngle  = 0;
float targetAngleFilt = 0;

// 调试: 原始 Pitch (供前后摆动时观察)
float rawAccelPitchDeg = 0;
float rawAccelAy = 0, rawAccelAz = 0;

// 控制状态
bool fallen = false;
bool diagMode = true;  // 开机默认诊断模式
int  stableCount = 0;

// 手机输入
float phoneX = 0;
float phoneY = 0;

// 电机反馈 (指令 + 0x02 帧解析)
int cmdSpdR = 0, cmdSpdL = 0;
float vinR = 0, vinL = 0;
int actualSpdR = 0, actualSpdL = 0;
float actualSpeedR = 0, actualSpeedL = 0;
float actualCurrentR = 0, actualCurrentL = 0;
float actualPosR = 0, actualPosL = 0;
int32_t encoderR = 0, encoderL = 0;
float motorTempR = 0, motorTempL = 0;
float linearSpeed = 0;
float distanceMM = 0;

uint32_t canTxFailCount = 0;

// 显示
int screenW = 0, screenH = 0;
String myIP = "";
