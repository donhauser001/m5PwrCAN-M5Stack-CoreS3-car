#pragma once
/**
 * can_motor.h — CAN 总线底层通信 + RollerCAN 电机控制
 */

#include <Arduino.h>

// CAN 初始化 (TWAI 驱动)
void canInit();

// 底层读写
int32_t readParam(uint8_t id, uint16_t reg);
bool    writeParam(uint8_t id, uint16_t reg, int32_t val);
void    flushCAN();

// 电机控制
void setMotorOutput(uint8_t id, bool on);
void setMotorCurrent(uint8_t id, int32_t mA);
void setMotorSpeed(uint8_t id, int32_t rpm);
void setMotorPosition(uint8_t id, int32_t deg_x100);
void setMotorModeAll(int mode);
void setMotorSpeedLoopGains(int32_t kp_reg, int32_t ki_reg, int32_t kd_reg);
void setMotorSpeedCurrentLimit(int32_t mA);
int  getMotorMode();
void driveMotors(int outR, int outL);  // 速度模式: 参数单位 RPM
void stopMotors();

// 扫描并初始化两个电机, 返回 true 表示全部OK
bool motorsInit();

// 轮询电机参数 (倒地时读CAN, 平衡时用指令值)
void motorsPollParams();
