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
#define ROBOT_WEIGHT_G         838  // 机器人总重量 (g)，含前后支架
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
// GAIN=6: PID线性区扩展到0-167RPM, P和D各有独立控制余量
// GAIN=10时P项在5.7°即饱和1000mA, D项完全无效 → 改为6后D项在小角度有充分制动力
#define CURRENT_MODE_GAIN_MA_PER_RPM 6.0f
#define CURRENT_MODE_LIMIT_MA        1000   // 短时可承受1A, 连续500mA; 平衡时脉冲式使用
// FOC 电机内部已处理低速/静摩擦，外部不再需要死区和限速
#define OUTPUT_DEADBAND_RPM       0   // 死区关闭: 任何 PID 输出都应立即传递给电机
#define MIN_EFFECTIVE_RPM         0   // 最小有效转速关闭: 不再跳变
#define OUTPUT_SLEW_RPM_PER_CYCLE 0   // 0 = 禁用斜率限制, PID 输出直达电机

// ============ 平衡方向 ============
// 如果机器人往倾斜方向跑(正反馈), 把这个值从 1 改成 -1 (或反过来)
#define BALANCE_DIR  (1)

// ============ PID 默认参数 (速度模式: 输出单位 RPM) ============
#define DEFAULT_KP   17.5f   // RPM/degree (实测最佳值, 配合GAIN=6: 1°→105mA, 重力66mA)
#define DEFAULT_KI   0.5f    // 消除稳态角度偏差; Ki*limit=10RPM封顶
#define DEFAULT_KD   1.40f   // RPM/(deg/s) 用户实测最佳值
#define INTEGRAL_LIMIT 20.0f  // 积分限幅 (deg·s); Ki*limit=10RPM封顶, 防止过冲
#define INTEGRAL_DECAY_THRESHOLD 2.0f  // |error|>此值时积分开始衰减, 大扰动时让P+D主导
#define INTEGRAL_DECAY_RATE      0.95f // 大误差时每周期积分乘以此值(快速衰减)
#define D_LIMIT        300.0f // D 项最大贡献 (RPM), 与 OUTPUT_LIMIT 一致, 保证D项充分制动

// ============ 速度补偿 (防漂移) ============
#define VELOCITY_K           0.10f   // 线速度→RPM增益 (恢复阶段已由startupGraceActive保护)
#define VELOCITY_CORR_LIMIT  15.0f   // 速度修正最大贡献(RPM)
#define VELOCITY_LPF_ALPHA   0.98f   // 速度低通滤波 (~0.25s响应, 恢复阶段归零不受影响)
// 偏航修正: 轮速和(旋转分量)反馈增益, 防止原地自旋
#define YAW_K                0.05f

// ============ 移动控制 (前进/转向) ============
#define MOVE_ANGLE_GAIN    0.03f   // phoneY(-100~100) -> targetAngle, max +-3 度
#define STEER_GAIN         0.15f   // phoneX(-100~100) -> steer RPM, max +-15 RPM
#define MOVE_INPUT_LPF     0.90f   // 移动输入低通 (平滑摇杆抖动+释放)
#define STEER_INPUT_LPF    0.85f   // 转向输入低通

// ============ IMU / 姿态 (6轴互补滤波, 无需校准) ============
// 实测: 竖直raw=+90°, 前倾极限raw=+77°(-13°), 后仰极限raw=+105°(+15°)
#define PITCH_MOUNT_OFFSET (-90.0f) // 安装偏移: raw竖直=+90°, 偏移-90→controlPitch=0
#define COMP_ALPHA     0.98f  // 互补滤波系数 (越大越信陀螺仪)
#define GYRO_LPF_ALPHA 0.5f   // 陀螺仪低通滤波 (0.5=更强平滑，抑制D项对gyro尖峰的过度放大)
#define TARGET_LPF_ALPHA 0.85f // 目标角低通 (越大越平滑)
#define FALL_ANGLE     14.0f  // 跌倒角度: 支架限位前-13°/后+15°, 14°在两侧极限之间
#define FALL_CONFIRM_COUNT 3   // 连续超阈值次数, 避免瞬时尖峰误判
#define STANDUP_MAX_ANGLE 17.0f  // 直接自立: 前倾极限-13°/后仰+15°, ±2°余量 → 最大17°

// ============ 启动安全门限 (直接自立) ============
// 按下 Stand 时: 若 pitch 在 ±STANDUP_MAX_ANGLE 内且角速度小、连续几拍静止即可启动
#define GYRO_START_THRESHOLD  8.0f   // 启动门限: pitch 轴角速度 (°/s)
#define STABLE_HOLD_COUNT     5      // 连续稳定采样次数 (5×5ms=25ms 静止即可 READY，直接自立)

// ============ 软启动 (仅正常角度启动时使用, 极限角度启动跳过) ============
#define SOFT_START_MS  100           // 斜坡时长 (ms)

// ============ 极限角度恢复模式 ============
// 从支架极限角度启动时, 使用更强PID全力回正, 回到安全区后切换回正常参数
#define RECOVERY_KP          20.0f  // 恢复模式 Kp: 全力拉回
#define RECOVERY_KD          4.0f   // 恢复模式 Kd: 强阻尼防止回正过冲
#define RECOVERY_ENTER_ANGLE 8.0f   // 恢复模式进入: 启动时|pitch|>此值即用恢复模式(覆盖前倾-12°)
#define RECOVERY_EXIT_ANGLE  5.0f   // 恢复模式退出: |pitch|<此值后切换回正常PID
#define STANDUP_GRACE_MS     800    // 保护期时长 (ms, 恢复期间不判跌倒)

// ============ 温度保护 ============
// 电机温度超过此值开始线性降额，最低保留 30% 输出
#define TEMP_THROTTLE_DEG  55.0f     // 降额起始温度 (°C)

// ============ 控制循环 ============
#define CTRL_HZ  200
#define CTRL_US  (1000000 / CTRL_HZ)

// ============ WiFi ============
#define WIFI_SSID_STR "aiden"
#define WIFI_PASS_STR "633234001"
