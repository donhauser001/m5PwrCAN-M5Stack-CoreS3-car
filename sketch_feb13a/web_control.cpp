#include "web_control.h"

#include "config.h"
#include "globals.h"
#include "web_protocol.h"
#include "web_ui_page.h"

#include <M5Unified.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

static WebServer httpServer(80);
static WebSocketsServer wsServer(81);

static void wsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
  case WStype_CONNECTED: {
    String pidMsg = buildWebPidMessage();
    String cfgMsg = buildWebConfigMessage();
    wsServer.sendTXT(num, pidMsg.c_str());
    wsServer.sendTXT(num, cfgMsg.c_str());
    break;
  }
  case WStype_DISCONNECTED:
    handleWebDisconnect();
    break;
  case WStype_TEXT: {
    String cmd;
    cmd.reserve(len + 1);
    for (size_t i = 0; i < len; i++) {
      cmd += (char)payload[i];
    }
    handleWebTextCommand(cmd);
    break;
  }
  default:
    break;
  }
}

void webInit() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.printf("WiFi: %s ...\n", WIFI_SSID_STR);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID_STR, WIFI_PASS_STR);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    myIP = WiFi.localIP().toString();
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.printf("IP: %s\n", myIP.c_str());
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("M5Bot", "12345678");
    myIP = WiFi.softAPIP().toString();
    M5.Lcd.setTextColor(RED);
    M5.Lcd.println("AP: M5Bot/12345678");
    M5.Lcd.printf("IP: %s\n", myIP.c_str());
  }

  httpServer.on("/", []() { httpServer.send_P(200, "text/html", WEB_INDEX_HTML); });
  httpServer.begin();

  wsServer.begin();
  wsServer.onEvent(wsEvent);
}

void webLoop() {
  httpServer.handleClient();
  wsServer.loop();
}

void webBroadcastAngle() {
  char msg[320];
  buildWebAngleMessage(msg, sizeof(msg));
  wsServer.broadcastTXT(msg);

  buildWebTelemetryMessage(msg, sizeof(msg));
  wsServer.broadcastTXT(msg);
}

void webBroadcastMotor() {
  char msg[160];
  buildWebMotorMessage(msg, sizeof(msg));
  wsServer.broadcastTXT(msg);
}

void webBroadcastText(const char* msg) {
  wsServer.broadcastTXT(msg);
}
