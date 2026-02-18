/**
 * display.cpp — CoreS3 LCD 屏幕显示
 */

#include "display.h"
#include "config.h"
#include "globals.h"
#include <M5Unified.h>

// ============ 绘制主 UI ============
void drawMainUI() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.setCursor(4, 4);
    M5.Lcd.print("Balance Bot CAN | ");
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.print(myIP);
}

// ============ 刷新动态数据 ============
void updateDisplay() {
    float displayPitch = currentPitch + PITCH_MOUNT_OFFSET;

    // --- 诊断模式: 大字显示原始 Pitch + 滤波 Pitch，方便找平衡点 ---
    if (diagMode) {
        M5.Lcd.fillRect(0, 20, screenW, screenH - 20, BLACK);
        M5.Lcd.setTextDatum(TL_DATUM);

        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(YELLOW, BLACK);
        M5.Lcd.setCursor(4, 24);
        M5.Lcd.printf("RAW Pitch");

        M5.Lcd.setTextSize(4);
        M5.Lcd.setTextColor(WHITE, BLACK);
        char buf[32];
        snprintf(buf, sizeof(buf), "%+7.2f", rawAccelPitchDeg);
        M5.Lcd.setCursor(4, 50);
        M5.Lcd.print(buf);

        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.setCursor(4, 100);
        M5.Lcd.printf("Filt: %+7.2f", displayPitch);

        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(CYAN, BLACK);
        M5.Lcd.setCursor(4, 130);
        M5.Lcd.printf("ay=%+.3f  az=%+.3f", (double)rawAccelAy, (double)rawAccelAz);

        M5.Lcd.setCursor(4, 148);
        M5.Lcd.printf("gyro=%+.1f  offset=%.1f", (double)gyroRate, (double)PITCH_MOUNT_OFFSET);

        M5.Lcd.setCursor(4, 166);
        uint16_t stColor = (stableCount >= STABLE_HOLD_COUNT) ? GREEN : YELLOW;
        M5.Lcd.setTextColor(stColor, BLACK);
        M5.Lcd.printf("stable=%d/%d  %s",
                       stableCount, STABLE_HOLD_COUNT,
                       (stableCount >= STABLE_HOLD_COUNT) ? "** READY **" : "");
        return;
    }

    // --- 平衡/倒地模式: 简洁运行视图 ---
    int barY = 30, barH = 40;
    M5.Lcd.fillRect(0, barY, screenW, barH + 30, BLACK);

    int barCenter = screenW / 2;
    int barLen = constrain((int)(displayPitch * screenW / 90.0), -barCenter, barCenter);

    uint16_t barColor = fallen ? RED : (fabs(displayPitch) < 5 ? GREEN : YELLOW);
    if (barLen > 0)
        M5.Lcd.fillRect(barCenter, barY, barLen, barH, barColor);
    else
        M5.Lcd.fillRect(barCenter + barLen, barY, -barLen, barH, barColor);
    M5.Lcd.drawLine(barCenter, barY, barCenter, barY + barH, WHITE);

    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextDatum(MC_DATUM);
    M5.Lcd.setTextColor(WHITE, BLACK);
    char buf[32];
    snprintf(buf, sizeof(buf), "%+.1f", displayPitch);
    M5.Lcd.drawString(buf, barCenter, barY + barH + 12);

    int infoY = barY + barH + 30;
    M5.Lcd.fillRect(0, infoY, screenW, screenH - infoY, BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextDatum(TL_DATUM);

    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.setCursor(4, infoY);
    M5.Lcd.printf("Kp=%.1f Ki=%.1f Kd=%.2f", Kp, Ki, Kd);

    M5.Lcd.setCursor(4, infoY + 14);
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.printf("Out=%+.0f  Target=%+.1f", pidOutput, targetAngleFilt);

    M5.Lcd.setCursor(4, infoY + 28);
    M5.Lcd.setTextColor(fallen ? RED : GREEN);
    M5.Lcd.printf("Status: %s", fallen ? "FALLEN" : "BALANCING");

    M5.Lcd.setCursor(4, infoY + 42);
    M5.Lcd.setTextColor(CYAN);
    M5.Lcd.printf("RPM R=%d L=%d  Act %.0f %.0f", cmdSpdR, cmdSpdL, (double)actualSpeedR, (double)actualSpeedL);

    M5.Lcd.setCursor(4, infoY + 56);
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.printf("I=%.0f/%.0fmA  %.0fmm/s", (double)actualCurrentR, (double)actualCurrentL, (double)linearSpeed);
}
