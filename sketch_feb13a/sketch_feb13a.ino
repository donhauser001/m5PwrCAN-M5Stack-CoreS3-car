/**
 * M5Stack CoreS3 自平衡机器人 — 主入口
 *
 * 模块结构:
 *   config.h        — 引脚/常量/宏定义
 *   globals.h/cpp   — 跨模块共享变量
 *   can_motor.h/cpp — CAN 总线 + 电机驱动
 *   imu_balance.h/cpp — IMU 姿态 + PID 平衡
 *   web_control.h/cpp — WiFi + WebSocket + 手机控制页
 *   display.h/cpp   — LCD 屏幕显示
 */

#include <M5Unified.h>
#include "config.h"
#include "globals.h"
#include "can_motor.h"
#include "imu_balance.h"
#include "web_control.h"
#include "display.h"

// ============ 时间管理 ============
static unsigned long lastCtrlUs  = 0;
static unsigned long lastDispMs  = 0;
static unsigned long lastWsMs    = 0;
static unsigned long lastMotorWsMs   = 0;
static unsigned long lastMotorPollMs = 0;
// (diagMode 由 Stand 按钮手动控制)

// ============ Setup ============
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);
    delay(300);

    // 屏幕
    M5.Lcd.setRotation(1);
    screenW = M5.Lcd.width();
    screenH = M5.Lcd.height();
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("Balance Bot CAN");

    Serial.println("\n========================================");
    Serial.println("  Balance Bot — CAN + IMU + WiFi");
    Serial.println("  R=0xA8  L=0xA9  TX=G17 RX=G18");
    Serial.printf("  BALANCE_DIR = %d\n", BALANCE_DIR);
    Serial.println("========================================\n");

    // IMU
    if (!M5.Imu.isEnabled()) {
        Serial.println("WARNING: IMU not found!");
    } else {
        Serial.println("IMU OK.");
    }

    // CAN
    canInit();

    // 电机
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.println("Scanning motors...");
    bool ok = motorsInit();
    M5.Lcd.printf("Motors: %s\n", ok ? "ALL OK" : "PARTIAL");

    // IMU 校准
    calibrateIMU();

    // WiFi + Web
    webInit();

    delay(1000);
    drawMainUI();
    lastCtrlUs = micros();

    // 进入诊断模式 (不驱动电机, 等待用户扶直)
    diagMode = true;

    Serial.println("╔══════════════════════════════════════╗");
    Serial.println("║  请手扶机器人竖直, 等 READY 出现     ║");
    Serial.println("║  然后按手机 Stand 按钮启动平衡       ║");
    Serial.println("╚══════════════════════════════════════╝");
    Serial.printf("PID: Kp=%.1f Ki=%.1f Kd=%.2f  DIR=%d\n", Kp, Ki, Kd, BALANCE_DIR);
    Serial.printf("MODE=SPEED  LIMIT=%dRPM  START_ANGLE=%.0f  Wheel=%dmm\n\n", OUTPUT_LIMIT, START_ANGLE, WHEEL_DIAMETER_MM);
}

// ============ Loop ============
void loop() {
    M5.update();
    webLoop();

    // --- 诊断模式: 等待手动按 Stand 启动 (不自动启动) ---

    // --- 高频控制循环 (200Hz) ---
    unsigned long nowUs = micros();
    if (nowUs - lastCtrlUs >= CTRL_US) {
        float dt = (nowUs - lastCtrlUs) / 1000000.0f;
        lastCtrlUs = nowUs;
        if (dt > 0.05f) dt = 0.05f;
        updateIMU(dt);
        balanceControl(dt);
    }

    // --- 触屏: 左=Kp-, 右=Kp+, 中=校准/站立 ---
    auto tc = M5.Touch.getDetail();
    if (tc.wasPressed()) {
        float controlPitch = currentPitch + PITCH_MOUNT_OFFSET;
        int third = screenW / 3;
        if (tc.x < third) {
            Kp = max(0.0f, Kp - 1.0f);
        } else if (tc.x > third * 2) {
            Kp = min(40.0f, Kp + 1.0f);
        } else {
            if (diagMode || fallen) {
                // 统一走安全启动路径 (角度 + 角速度 + 连续稳定窗口)
                if (!activateBalance()) {
                    // 条件不满足时确保退到诊断模式，串口已打印原因
                    diagMode = true;
                    fallen   = false;
                }
            } else {
                calibrateIMU();
                drawMainUI();
            }
        }
    }

    // --- 低频任务 ---
    unsigned long nowMs = millis();

    // 屏幕刷新 (150ms)
    if (nowMs - lastDispMs > 150) {
        lastDispMs = nowMs;
        updateDisplay();
    }

    // WebSocket 姿态广播 (20ms → 50Hz，捕捉起步瞬间动态)
    if (nowMs - lastWsMs > 20) {
        lastWsMs = nowMs;
        webBroadcastAngle();
    }

    // 电机状态广播 (250ms)
    if (nowMs - lastMotorWsMs > 250) {
        lastMotorWsMs = nowMs;
        webBroadcastMotor();
    }

    // 电机参数轮询 (仅诊断/倒地时，避免平衡期阻塞)
    if (nowMs - lastMotorPollMs > 1000) {
        lastMotorPollMs = nowMs;
        motorsPollParams();
    }
}
