#pragma once
/**
 * web_control.h — WiFi 连接 + HTTP 服务 + WebSocket 实时通信
 */

#include <Arduino.h>

// WiFi + Web 服务器初始化
void webInit();

// 主循环中调用 (处理 HTTP 和 WebSocket)
void webLoop();

// 广播姿态数据到手机
void webBroadcastAngle();

// 广播电机参数到手机
void webBroadcastMotor();
