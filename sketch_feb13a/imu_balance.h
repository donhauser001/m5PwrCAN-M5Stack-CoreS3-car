#pragma once
/**
 * imu_balance.h — IMU 姿态估算 + PID 自平衡控制
 */

// IMU 校准 (采集500次取平均, 约2秒)
void calibrateIMU();

// 姿态更新 (互补滤波, 每个控制周期调用)
void updateIMU(float dt);

// PID 平衡控制 (计算电机输出并驱动)
void balanceControl(float dt);

// 安全启动平衡: 同时检查角度+角速度+连续稳定窗口，满足条件才激活。
// 成功返回 true，条件不满足返回 false (diagMode 保持)。
bool activateBalance();

// 架空轮静态阶跃实验: true=启动 0~30RPM 阶跃扫描, false=停止并清零
void setBenchStepTest(bool on);
