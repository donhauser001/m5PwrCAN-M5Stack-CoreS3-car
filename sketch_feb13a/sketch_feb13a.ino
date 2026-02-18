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
#include "auto_tune.h"

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

    // IMU
    if (!M5.Imu.isEnabled()) {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println("IMU not found!");
    }

    // CAN
    canInit();

    // 电机
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.println("Scanning motors...");
    bool ok = motorsInit();
    M5.Lcd.printf("Motors: %s\n", ok ? "ALL OK" : "PARTIAL");


    // WiFi + Web
    webInit();

    delay(1000);
    drawMainUI();
    lastCtrlUs = micros();

    // 进入诊断模式 (不驱动电机, 等待用户扶直)
    diagMode = true;

}

// ============ Loop ============
void loop() {
    M5.update();

    // --- 高频控制循环 (固定步长 200Hz) ---
    // 固定 dt 可避免网络/串口偶发阻塞造成的等效参数漂移与控制突变。
    unsigned long nowUs = micros();
    int catchup = 0;
    while ((nowUs - lastCtrlUs) >= CTRL_US && catchup < 6) {
        lastCtrlUs += CTRL_US;
        const float dt = CTRL_US / 1000000.0f;
        updateIMU(dt);
        balanceControl(dt);
        catchup++;
    }
    if ((nowUs - lastCtrlUs) >= CTRL_US) {
        lastCtrlUs = nowUs;
    }

    // --- 网络任务 (平衡时减少处理，降低阻塞) ---
    webLoop();

    // --- 网络任务后再补一轮控制 (减少 WiFi 引起的控制空白) ---
    nowUs = micros();
    catchup = 0;
    while ((nowUs - lastCtrlUs) >= CTRL_US && catchup < 4) {
        lastCtrlUs += CTRL_US;
        const float dt = CTRL_US / 1000000.0f;
        updateIMU(dt);
        balanceControl(dt);
        catchup++;
    }
    if ((nowUs - lastCtrlUs) >= CTRL_US) {
        lastCtrlUs = nowUs;
    }

    // --- 触屏: 左=Kp-, 右=Kp+, 中=站立 (自动调参时屏蔽) ---
    auto tc = M5.Touch.getDetail();
    if (tc.wasPressed() && !isAutoTuning()) {
        int third = screenW / 3;
        if (tc.x < third) {
            Kp = max(0.0f, Kp - 1.0f);
        } else if (tc.x > third * 2) {
            Kp = min(40.0f, Kp + 1.0f);
        } else {
            if (fallen) {
                diagMode = true;
                fallen   = false;
                stableCount = STABLE_HOLD_COUNT;
            }
            if (diagMode) {
                activateBalance();
            }
        }
    }

    // --- 自动调参状态机 ---
    autoTuneUpdate();

    // --- 低频任务 ---
    unsigned long nowMs = millis();

    // 屏幕刷新 — 平衡期完全跳过 (SPI操作约30-40ms，严重阻塞控制环)
    bool balancing = !diagMode && !fallen && !benchMode;
    if (!balancing && (nowMs - lastDispMs > 500)) {
        lastDispMs = nowMs;
        updateDisplay();
    }

    // WebSocket 姿态广播 (50ms → 20Hz, 从20ms提高到50ms减少WiFi阻塞)
    if (nowMs - lastWsMs > 50) {
        lastWsMs = nowMs;
        webBroadcastAngle();
    }

    // 电机状态广播 (500ms)
    if (nowMs - lastMotorWsMs > 500) {
        lastMotorWsMs = nowMs;
        webBroadcastMotor();
    }

    // 电机参数轮询 (仅诊断/倒地时，避免平衡期阻塞)
    if (nowMs - lastMotorPollMs > 1000) {
        lastMotorPollMs = nowMs;
        motorsPollParams();
    }
}
