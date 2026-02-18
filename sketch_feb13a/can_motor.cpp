/**
 * can_motor.cpp — CAN 总线底层通信 + RollerCAN 电机控制
 */

#include "can_motor.h"
#include "config.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "globals.h"

// ============ 轮询状态 ============
static int motorReadIdx = 0;
static const int READ_TIMEOUT_MS = 15;
static const uint8_t FEEDBACK_CMD = 0x02;
static void parseFeedback(const twai_message_t *rx);
static int gMotorMode = MODE_SPEED;
static unsigned long lastFeedbackMsR = 0, lastFeedbackMsL = 0;
static const unsigned long FEEDBACK_STALE_MS = 20;

// ============ CAN 底层 ============
static bool canSend(uint8_t id, uint8_t cmd, uint16_t opt, uint8_t *d, int txTimeoutMs = 5) {
  twai_message_t m = {};
  m.identifier = ((uint32_t)cmd << 24) | ((uint32_t)opt << 16) | id;
  m.data_length_code = 8;
  m.flags = TWAI_MSG_FLAG_EXTD;
  if (d)
    memcpy(m.data, d, 8);
  return twai_transmit(&m, pdMS_TO_TICKS(txTimeoutMs)) == ESP_OK;
}

static bool canRecv(twai_message_t *rx, int ms) {
  return twai_receive(rx, pdMS_TO_TICKS(ms)) == ESP_OK;
}

static bool isExtFrame(const twai_message_t *rx) {
  return (rx->flags & TWAI_MSG_FLAG_EXTD) != 0;
}

static uint8_t frameCmd(const twai_message_t *rx) {
  return (uint8_t)((rx->identifier >> 24) & 0x1F);
}

// ============ CAN 初始化 ============
void canInit() {
  // 先清理可能残留的 TWAI 驱动
  gpio_reset_pin((gpio_num_t)CAN_TX_PIN);
  gpio_reset_pin((gpio_num_t)CAN_RX_PIN);
  {
    twai_general_config_t gd = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)6, (gpio_num_t)7, TWAI_MODE_NORMAL);
    twai_timing_config_t td = TWAI_TIMING_CONFIG_1MBITS();
    twai_filter_config_t fd = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if (twai_driver_install(&gd, &td, &fd) == ESP_OK) {
      twai_start();
      delay(100);
      twai_stop();
      twai_driver_uninstall();
    }
    delay(100);
  }
  gpio_reset_pin((gpio_num_t)CAN_TX_PIN);
  gpio_reset_pin((gpio_num_t)CAN_RX_PIN);

  // 安装目标引脚
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
      (gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  g.rx_queue_len = 10;
  g.tx_queue_len = 5;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_1MBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  twai_driver_install(&g, &t, &f);
  twai_start();
  delay(500);
  flushCAN();
}

// ============ 参数读写 ============
int32_t readParam(uint8_t id, uint16_t reg) {
  uint8_t d[8] = {};
  d[0] = reg & 0xFF;
  d[1] = (reg >> 8) & 0xFF;
  if (!canSend(id, CMD_READ, 0, d, 10))
    return -99999;

  unsigned long startMs = millis();
  while ((int)(millis() - startMs) < READ_TIMEOUT_MS) {
    twai_message_t rx;
    int remainMs = READ_TIMEOUT_MS - (int)(millis() - startMs);
    if (remainMs < 1)
      break;
    if (!canRecv(&rx, remainMs))
      break;
    if (!isExtFrame(&rx) || rx.data_length_code < 8)
      continue;

    // 反馈帧先交给反馈解析，避免污染参数读取路径
    if (frameCmd(&rx) == FEEDBACK_CMD) {
      parseFeedback(&rx);
      continue;
    }

    // 参数读回包必须匹配: 电机ID + 寄存器地址
    uint8_t rxMotorId = (uint8_t)((rx.identifier >> 8) & 0xFF);
    uint16_t rxReg = (uint16_t)rx.data[0] | ((uint16_t)rx.data[1] << 8);
    if (rxMotorId != id || rxReg != reg)
      continue;

    return (int32_t)rx.data[4] | ((int32_t)rx.data[5] << 8) |
           ((int32_t)rx.data[6] << 16) | ((int32_t)rx.data[7] << 24);
  }
  return -99999;
}

bool writeParam(uint8_t id, uint16_t reg, int32_t val) {
  uint8_t d[8] = {};
  d[0] = reg & 0xFF;
  d[1] = (reg >> 8) & 0xFF;
  d[4] = val & 0xFF;
  d[5] = (val >> 8) & 0xFF;
  d[6] = (val >> 16) & 0xFF;
  d[7] = (val >> 24) & 0xFF;
  if (!canSend(id, CMD_WRITE, 0, d, 10))
    return false;
  twai_message_t rx;
  canRecv(&rx, 20);
  return true;
}

void flushCAN() {
  twai_message_t rx;
  while (twai_receive(&rx, pdMS_TO_TICKS(1)) == ESP_OK) {
  }
}

// ============ 0x02 反馈帧解析 ============
// 应答帧 ID: bit28-24=0x02, bit15-8=电机CAN ID; 数据: Byte0-1 速度RPM, 2-3
// 位置°, 4-5 电流mA, 6-7 电压V
static void parseFeedback(const twai_message_t *rx) {
  if (!isExtFrame(rx))
    return;
  if (rx->data_length_code < 8)
    return;
  if (frameCmd(rx) != FEEDBACK_CMD)
    return;

  uint8_t motorId = (rx->identifier >> 8) & 0xFF;
  if (motorId != MOTOR_R && motorId != MOTOR_L)
    return;

  int16_t spd = (int16_t)((uint16_t)rx->data[0] | ((uint16_t)rx->data[1] << 8));
  int16_t pos = (int16_t)((uint16_t)rx->data[2] | ((uint16_t)rx->data[3] << 8));
  int16_t cur = (int16_t)((uint16_t)rx->data[4] | ((uint16_t)rx->data[5] << 8));
  int16_t vol = (int16_t)((uint16_t)rx->data[6] | ((uint16_t)rx->data[7] << 8));

  // 防止错帧污染: 速度/电压超出物理范围直接丢弃
  if (abs(spd) > 6000)
    return;
  if (vol < 0 || vol > 3000)
    return;

  unsigned long nowMs = millis();
  if (motorId == MOTOR_R) {
    actualSpeedR = (float)spd;
    actualSpdR = spd;
    actualPosR = (float)pos;
    actualCurrentR = (float)cur;
    vinR = (float)vol;
    lastFeedbackMsR = nowMs;
  } else if (motorId == MOTOR_L) {
    actualSpeedL = (float)spd;
    actualSpdL = spd;
    actualPosL = (float)pos;
    actualCurrentL = (float)cur;
    vinL = (float)vol;
    lastFeedbackMsL = nowMs;
  }
}

// ============ 电机控制 ============
void setMotorOutput(uint8_t id, bool on) {
  uint8_t d[8] = {};
  canSend(id, on ? CMD_ON : CMD_OFF, 0, d, 10);
  twai_message_t rx;
  if (canRecv(&rx, 100))
    parseFeedback(&rx);
}

void setMotorSpeed(uint8_t id, int32_t rpm) {
  uint8_t d[8] = {};
  int32_t val = rpm * 100;
  d[0] = REG_SPEED & 0xFF;
  d[1] = (REG_SPEED >> 8) & 0xFF;
  d[4] = val & 0xFF;
  d[5] = (val >> 8) & 0xFF;
  d[6] = (val >> 16) & 0xFF;
  d[7] = (val >> 24) & 0xFF;
  // 控制环中优先低延迟，队列满时宁可丢一帧也不阻塞
  canSend(id, CMD_WRITE, 0, d, 0);
}

static void setMotorSpeedReliable(uint8_t id, int32_t rpm) {
  uint8_t d[8] = {};
  int32_t val = rpm * 100;
  d[0] = REG_SPEED & 0xFF;
  d[1] = (REG_SPEED >> 8) & 0xFF;
  d[4] = val & 0xFF;
  d[5] = (val >> 8) & 0xFF;
  d[6] = (val >> 16) & 0xFF;
  d[7] = (val >> 24) & 0xFF;
  // 安全路径: 关键停机/初始化时确保送达
  canSend(id, CMD_WRITE, 0, d, 10);
}

void setMotorCurrent(uint8_t id, int32_t mA) {
  uint8_t d[8] = {};
  int32_t val = mA * 100;
  d[0] = REG_CURRENT & 0xFF;
  d[1] = (REG_CURRENT >> 8) & 0xFF;
  d[4] = val & 0xFF;
  d[5] = (val >> 8) & 0xFF;
  d[6] = (val >> 16) & 0xFF;
  d[7] = (val >> 24) & 0xFF;
  // 非阻塞: 500Hz(2ms)周期容不下阻塞等待, 队列满时丢帧而非卡死控制环
  if (!canSend(id, CMD_WRITE, 0, d, 0))
    canTxFailCount++;
}

void setMotorPosition(uint8_t id, int32_t deg_x100) {
  uint8_t d[8] = {};
  d[0] = REG_POSITION & 0xFF;
  d[1] = (REG_POSITION >> 8) & 0xFF;
  d[4] = deg_x100 & 0xFF;
  d[5] = (deg_x100 >> 8) & 0xFF;
  d[6] = (deg_x100 >> 16) & 0xFF;
  d[7] = (deg_x100 >> 24) & 0xFF;
  canSend(id, CMD_WRITE, 0, d, 0);
}

void setMotorModeAll(int mode) {
  if (mode != MODE_SPEED && mode != MODE_CURRENT && mode != MODE_POSITION) {
    return;
  }
  uint8_t ids[] = {MOTOR_R, MOTOR_L};
  for (int i = 0; i < 2; i++) {
    writeParam(ids[i], REG_MODE, mode);
    delay(20);
  }
  gMotorMode = mode;
}

void setMotorSpeedLoopGains(int32_t kp_reg, int32_t ki_reg, int32_t kd_reg) {
  uint8_t ids[] = {MOTOR_R, MOTOR_L};
  for (int i = 0; i < 2; i++) {
    writeParam(ids[i], REG_SPEED_KP, kp_reg);
    delay(20);
    writeParam(ids[i], REG_SPEED_KI, ki_reg);
    delay(20);
    writeParam(ids[i], REG_SPEED_KD, kd_reg);
    delay(20);
  }
}

void setMotorSpeedCurrentLimit(int32_t mA) {
  uint8_t ids[] = {MOTOR_R, MOTOR_L};
  for (int i = 0; i < 2; i++) {
    writeParam(ids[i], REG_SPEED_MAX_CUR, mA * 100);
    delay(20);
  }
}

int getMotorMode() {
  return gMotorMode;
}

// 统一处理接收缓冲区中的所有反馈帧 (非阻塞)
static void processAllFeedback() {
  twai_message_t rx;
  // 循环读取直到缓冲区空, 超时设为 0 (非阻塞)
  while (twai_receive(&rx, 0) == ESP_OK) {
    parseFeedback(&rx);
  }
}

void driveMotors(int outR, int outL) {
  // 发送指令 (非阻塞). 平衡默认速度模式; 其余模式用于实验扩展。
  if (gMotorMode == MODE_CURRENT) {
    int curR = (int)constrain(outR * CURRENT_MODE_GAIN_MA_PER_RPM, -(float)CURRENT_MODE_LIMIT_MA, (float)CURRENT_MODE_LIMIT_MA);
    int curL = (int)constrain(outL * CURRENT_MODE_GAIN_MA_PER_RPM, -(float)CURRENT_MODE_LIMIT_MA, (float)CURRENT_MODE_LIMIT_MA);
    setMotorCurrent(MOTOR_R, curR * DIR_R);
    setMotorCurrent(MOTOR_L, curL * DIR_L);
  } else {
    setMotorSpeed(MOTOR_R, outR * DIR_R);
    setMotorSpeed(MOTOR_L, outL * DIR_L);
  }

  // 集中处理反馈 (非阻塞)
  processAllFeedback();

  // 反馈新鲜度: 若某电机超过 20ms 未更新, 用另一侧镜像避免 yaw 误算导致 L/R 分裂
  unsigned long nowMs = millis();
  if (lastFeedbackMsR > 0 && (nowMs - lastFeedbackMsR) > FEEDBACK_STALE_MS) {
    actualSpeedR = actualSpeedL;
    actualSpdR = actualSpdL;
  }
  if (lastFeedbackMsL > 0 && (nowMs - lastFeedbackMsL) > FEEDBACK_STALE_MS) {
    actualSpeedL = actualSpeedR;
    actualSpdL = actualSpdR;
  }

  // 更新线速度 (mm/s): DIR_L=-1 → 左轮正转报告负RPM, 用差值才是平移速度
  linearSpeed = ((float)actualSpeedR - (float)actualSpeedL) * 0.5f *
                (float)WHEEL_CIRCUMFERENCE_MM / 60.0f;
}

void stopMotors() {
  if (gMotorMode == MODE_CURRENT) {
    for (int i = 0; i < 3; i++) {
      setMotorCurrent(MOTOR_R, 0);
      setMotorCurrent(MOTOR_L, 0);
      delay(2);
    }
  } else {
    for (int i = 0; i < 3; i++) {
      setMotorSpeedReliable(MOTOR_R, 0);
      setMotorSpeedReliable(MOTOR_L, 0);
      delay(2);
    }
  }
  linearSpeed = 0;
  flushCAN();
}

// ============ 扫描 + 初始化电机 ============
bool motorsInit() {
  int32_t vr = readParam(MOTOR_R, REG_VIN);
  bool rOk = (vr != -99999 && vr > 0);
  int32_t vl = readParam(MOTOR_L, REG_VIN);
  bool lOk = (vl != -99999 && vl > 0);


  if (rOk)
    vinR = vr / 100.0f;
  if (lOk)
    vinL = vl / 100.0f;

  uint8_t ids[] = {MOTOR_R, MOTOR_L};

  // 步骤1: 先关闭输出 + 清零速度设定点，防止上电时电机用残留值起转
  for (int i = 0; i < 2; i++) {
    setMotorOutput(ids[i], false); // CMD_OFF: 禁用输出
    delay(20);
    setMotorSpeedReliable(ids[i], 0); // 清零速度设定点(可靠发送)
    delay(20);
  }

  // 步骤2: 配置为电流(力矩)模式 — 消除电机内部速度环PID延迟,
  //         让平衡控制器直接控制力矩, 响应时间从50-100ms降至<1ms.
  setMotorModeAll(MODE_CURRENT);

  // 步骤3: 确认电流设定点为 0，再开启输出
  for (int i = 0; i < 2; i++) {
    setMotorCurrent(ids[i], 0);    // 电流模式: 清零力矩指令
    delay(10);
    setMotorOutput(ids[i], true);  // CMD_ON: 此时设定点已为 0，不会起转
    delay(20);
  }
  stopMotors();

  return rOk && lOk;
}

// ============ 轮询电机参数 (低频: 温度/电压/编码器) ============
void motorsPollParams() {
  // 平衡期不做阻塞参数查询，避免打断 200Hz 控制环
  if (!diagMode && !fallen)
    return;

  switch (motorReadIdx) {
  case 0: {
    int32_t v = readParam(MOTOR_R, REG_VIN);
    if (v != -99999)
      vinR = v / 100.0f;
    break;
  }
  case 1: {
    int32_t v = readParam(MOTOR_L, REG_VIN);
    if (v != -99999)
      vinL = v / 100.0f;
    break;
  }
  case 2: {
    int32_t v = readParam(MOTOR_R, REG_TEMP);
    if (v != -99999)
      motorTempR = (float)v;
    break;
  }
  case 3: {
    int32_t v = readParam(MOTOR_L, REG_TEMP);
    if (v != -99999)
      motorTempL = (float)v;
    break;
  }
  case 4: {
    int32_t v = readParam(MOTOR_R, REG_ENCODER);
    if (v != -99999)
      encoderR = v;
    break;
  }
  case 5: {
    int32_t v = readParam(MOTOR_L, REG_ENCODER);
    if (v != -99999)
      encoderL = v;
    break;
  }
  }
  motorReadIdx = (motorReadIdx + 1) % 6;
}
