#include "web_protocol.h"

#include "auto_tune.h"
#include "can_motor.h"
#include "config.h"
#include "display.h"
#include "globals.h"
#include "imu_balance.h"

String buildWebPidMessage() {
  return "P," + String(Kp, 1) + "," + String(Ki, 1) + "," + String(Kd, 2);
}

String buildWebConfigMessage() {
  return "C," + String(ROBOT_WEIGHT_G) + "," + String(WHEEL_DIAMETER_MM);
}

void handleWebDisconnect() {
  phoneX = 0;
  phoneY = 0;
  targetAngle = 0;
  targetAngleFilt = 0;
}

void handleWebTextCommand(const String &cmd) {
  if (cmd == "AT") {
    autoTuneStart();
    return;
  }
  if (cmd == "AX") {
    autoTuneStop();
    return;
  }
  if (isAutoTuning()) return;

  if (cmd.startsWith("J,")) {
    int jx = 0;
    int jy = 0;
    sscanf(cmd.c_str() + 2, "%d,%d", &jx, &jy);
    phoneX = -jx;
    phoneY = -jy;
    targetAngle = phoneY * MOVE_ANGLE_GAIN;
    return;
  }

  if (cmd.startsWith("P,")) {
    float np = 0;
    float ni = 0;
    float nd = 0;
    sscanf(cmd.c_str() + 2, "%f,%f,%f", &np, &ni, &nd);
    Kp = np;
    Ki = ni;
    Kd = nd;
    pidIntegral = 0;
    return;
  }

  // 内部速度环参数 (RollerCAN FOC内环)
  if (cmd.startsWith("G,")) {
    int32_t ikp = 0, iki = 0, ikd = 0;
    sscanf(cmd.c_str() + 2, "%ld,%ld,%ld", &ikp, &iki, &ikd);
    setMotorSpeedLoopGains(ikp, iki, ikd);
    return;
  }

  // 速度模式限流 (mA)
  if (cmd.startsWith("IL,")) {
    int32_t mA = 0;
    sscanf(cmd.c_str() + 3, "%ld", &mA);
    if (mA < 200) mA = 200;
    if (mA > 3000) mA = 3000;
    setMotorSpeedCurrentLimit(mA);
    return;
  }

  // 电机模式切换
  if (cmd == "MS") {
    setMotorModeAll(MODE_SPEED);
    return;
  }
  if (cmd == "MC") {
    setMotorModeAll(MODE_CURRENT);
    return;
  }
  if (cmd == "MP") {
    setMotorModeAll(MODE_POSITION);
    return;
  }

  if (cmd == "R") {
    setBenchStepTest(false);
    stopMotors();
    diagMode = true;
    fallen = false;
    drawMainUI();
    return;
  }

  if (cmd == "S") {
    setBenchStepTest(false);
    if (getMotorMode() != MODE_CURRENT) {
      setMotorModeAll(MODE_CURRENT);
    }
    if (fallen) {
      diagMode = true;
      fallen   = false;
      stableCount = STABLE_HOLD_COUNT;
    }
    activateBalance();
    return;
  }

  if (cmd == "B,1") {
    setBenchStepTest(true);
    return;
  }

  if (cmd == "B,0") {
    setBenchStepTest(false);
    return;
  }

  if (cmd == "E") {
    setBenchStepTest(false);
    diagMode = false;
    fallen = true;
    stopMotors();
    pidIntegral = 0;
  }
}

void buildWebAngleMessage(char *msg, size_t size) {
  float controlPitch = currentPitch + PITCH_MOUNT_OFFSET;

  // A,pitch,roll,yaw,pid,fallen,diag,target,gyro,linearSpeed(mm/s),distance(mm),cmdR,cmdL,actR,actL,benchMode
  snprintf(msg, size,
           "A,%.2f,%.2f,%.2f,%.1f,%d,%d,%.2f,%.2f,%.1f,%.1f,%d,%d,%d,%d,%d",
           controlPitch, currentRoll, currentYaw, pidOutput, fallen ? 1 : 0,
           diagMode ? 1 : 0, targetAngleFilt, gyroRate, linearSpeed, distanceMM,
           cmdSpdR, cmdSpdL, actualSpdR, actualSpdL, benchMode ? 1 : 0);
}

void buildWebTelemetryMessage(char *msg, size_t size) {
  float controlPitch = currentPitch + PITCH_MOUNT_OFFSET;
  unsigned long nowMs = millis();

  // T,...,benchMode,canTxFailCount
  snprintf(msg, size,
           "T,%lu,%.2f,%.2f,%.2f,%.1f,%d,%d,%d,%d,%.3f,%.4f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%d,%d,%.2f,%.1f,%.1f,%.0f,%d,%d,%d,%lu",
           nowMs, controlPitch, targetAngleFilt, gyroRate, pidOutput, cmdSpdR, cmdSpdL,
           actualSpdR, actualSpdL, linearSpeed / 1000.0f, distanceMM / 1000.0f,
           vinR, vinL, actualCurrentR, actualCurrentL, motorTempR, motorTempL,
           fallen ? 1 : 0, diagMode ? 1 : 0,
           ctrlDtMs, dbgPidRaw, dbgPidClamped, dbgAfterDeadzone, dbgSentR, dbgSentL, benchMode ? 1 : 0, canTxFailCount);
}

void buildWebMotorMessage(char *msg, size_t size) {
  snprintf(msg, size,
           "M,%d,%d,%d,%d,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f",
           cmdSpdR, cmdSpdL, actualSpdR, actualSpdL,
           vinR, vinL, actualCurrentR, actualCurrentL,
           motorTempR, motorTempL);
}
