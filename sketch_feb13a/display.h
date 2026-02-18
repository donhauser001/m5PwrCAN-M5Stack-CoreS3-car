#pragma once
/**
 * display.h — CoreS3 LCD 屏幕显示
 */

// 绘制主 UI 框架 (标题栏 + IP)
void drawMainUI();

// 刷新动态数据 (角度条、PID、状态)
void updateDisplay();
