#pragma once
/**
 * config.h — 全局配置: 引脚、常量、宏定义
 */

// ============ CAN 引脚 ============
#define CAN_TX_PIN 17
#define CAN_RX_PIN 18

// ============ 电机 CAN ID ============
#define MOTOR_R 0xA8
#define MOTOR_L 0xA9

// ============ CAN 协议命令字 ============
#define CMD_ON    0x03
#define CMD_OFF   0x04
#define CMD_READ  0x11
#define CMD_WRITE 0x12

// ============ RollerCAN 寄存器 ============
#define REG_MODE           0x7005
#define REG_CURRENT        0x7006  // 电流指令 (mA×100)
#define REG_SPEED           0x700A  // 速度指令 (RPM×100)
#define REG_POSITION        0x7016  // 位置指令 (°×100)
#define REG_POS_MAX_CUR    0x7017  // 位置模式最大电流 (mA×100)
#define REG_SPEED_MAX_CUR  0x7018  // 速度模式最大电流 (mA×100)
#define REG_SPEED_KP        0x7020  // 速度环 Kp (×100000)
#define REG_SPEED_KI        0x7021  // 速度环 Ki (×10000000)
#define REG_SPEED_KD        0x7022  // 速度环 Kd (×100000)
#define REG_SPEED_ACTUAL    0x7030  // 实时速度读回 (RPM×100)
#define REG_POS_READBACK    0x7031  // 实时位置读回 (°×100)
#define REG_CUR_READBACK    0x7032  // 实时电流读回 (mA×100)
#define REG_ENCODER         0x7033  // 编码器计数 (36000=360°)
#define REG_VIN             0x7034  // 输入电压 (V×100)
#define REG_TEMP            0x7035  // 芯片温度 °C

// ============ 车轮物理参数 ============
#define ROBOT_WEIGHT_G         830  // 机器人总重量 (g)
#define WHEEL_DIAMETER_MM      85
#define WHEEL_CIRCUMFERENCE_MM 267  // π*85 ≈ 267
#define ENCODER_COUNTS_PER_REV 36000

// ============ 电机参数 ============
#define MODE_SPEED   1       // 速度模式 — 内部 FOC 速度闭环
#define MODE_POSITION 2      // 位置模式
#define MODE_CURRENT 3       // 电流模式 (备用)
#define DIR_R        ( 1)    // 右轮方向 (倒了就交换正负)
#define DIR_L        (-1)    // 左轮方向
#define OUTPUT_LIMIT 300     // 速度模式: RPM 上限
#define SPEED_MAX_CURRENT 800  // 速度模式最大电流 mA (电机连续额定500mA，短时额定1000mA，取800mA为动态余量上限)
// 内部速度环参数 (写入电机驱动寄存器)
// 协议缩放: Kp×100000, Ki×10000000, Kd×100000
#define MOTOR_SPEED_LOOP_KP  250000   // 2.5
#define MOTOR_SPEED_LOOP_KI  500000   // 0.05
#define MOTOR_SPEED_LOOP_KD  5000     // 0.05
// 电流模式下, 平衡输出(RPM)到电流命令(mA)的线性映射
#define CURRENT_MODE_GAIN_MA_PER_RPM 35.0f
#define CURRENT_MODE_LIMIT_MA        1800
// 克服静摩擦: 小输出先置零，超过阈值后抬升到最小有效转速
#define OUTPUT_DEADBAND_RPM 4
#define MIN_EFFECTIVE_RPM  6
#define OUTPUT_SLEW_RPM_PER_CYCLE 3

// ============ 平衡方向 ============
// 如果机器人往倾斜方向跑(正反馈), 把这个值从 1 改成 -1 (或反过来)
#define BALANCE_DIR  (1)

// ============ PID 默认参数 (速度模式: 输出单位 RPM) ============
#define DEFAULT_KP   8.0f    // RPM/degree
#define DEFAULT_KI   0.0f    // 先关闭, 稳定后可加
#define DEFAULT_KD   0.45f   // RPM/(deg/s) (加大D阻尼，抑制12V低扭矩下的震荡)
#define INTEGRAL_LIMIT 80.0f  // 积分限幅 (RPM·s)
#define D_LIMIT        120.0f // D 项最大贡献 (RPM)

// ============ 速度补偿 (真实编码器速度防漂移) ============
// 线速度 mm/s → 扣减 RPM 的增益
#define VELOCITY_K   0.00f   // 先关闭, 避免速度噪声耦合

// ============ IMU / 姿态 (6轴互补滤波) ============
#define COMP_ALPHA     0.98f  // 互补滤波系数 (越大越信陀螺仪)
#define GYRO_LPF_ALPHA 0.5f   // 陀螺仪低通滤波 (D项去噪, 0.5=更快响应，配合加大的Kd)
#define TARGET_LPF_ALPHA 0.85f // 目标角低通 (越大越平滑)
#define FALL_ANGLE     12.0f  // 跌倒角度 (12V@低电时电机扭矩不足以在14°纠偏，提前放弃)
#define FALL_CONFIRM_COUNT 2   // 连续超阈值次数(200Hz下约10ms)后判定跌倒
#define START_ANGLE     6.0f  // 启动角度阈值 (从更小角度启动，保留更多扭矩余量)
#define PITCH_MOUNT_OFFSET 0.0f  // CoreS3 安装角度校正 (逆时针90° 可试 -90 或 90)

// ============ 启动安全门限 ============
// 按下 Stand 前，pitch 角速度必须连续 STABLE_HOLD_COUNT 个控制周期低于此阈值
#define GYRO_START_THRESHOLD  8.0f   // 启动门限: pitch 轴角速度 (°/s)
#define STABLE_HOLD_COUNT     30     // 连续稳定采样次数 (30×5ms = 150ms)

// ============ 软启动 ============
// 启动后输出从 0 线性斜坡爬升到全幅，避免电机突转冲击
#define SOFT_START_MS  80            // 斜坡时长 (ms，过长会在软启动期间漂移倒下)

// ============ 温度保护 ============
// 电机温度超过此值开始线性降额，最低保留 30% 输出
#define TEMP_THROTTLE_DEG  55.0f     // 降额起始温度 (°C)

// ============ 控制循环 ============
#define CTRL_HZ  200
#define CTRL_US  (1000000 / CTRL_HZ)

// ============ WiFi ============
#define WIFI_SSID_STR "aiden"
#define WIFI_PASS_STR "633234001"
