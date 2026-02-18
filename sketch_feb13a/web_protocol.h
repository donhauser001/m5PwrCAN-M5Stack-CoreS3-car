#pragma once

#include <Arduino.h>

String buildWebPidMessage();
String buildWebConfigMessage();

void handleWebDisconnect();
void handleWebTextCommand(const String &cmd);

void buildWebAngleMessage(char *msg, size_t size);
void buildWebTelemetryMessage(char *msg, size_t size);
void buildWebMotorMessage(char *msg, size_t size);
