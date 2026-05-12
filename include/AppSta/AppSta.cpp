#include "AppSta.h"

#include <ESP8266WiFi.h>

#include <WifiStaStore.h>

namespace {

constexpr uint32_t kReconnectMs = 10000;

uint32_t s_lastReconnectAttempt = 0;

}  // namespace

bool appStaIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void appStaLoop() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  const uint32_t now = millis();
  if (now - s_lastReconnectAttempt < kReconnectMs) {
    return;
  }
  s_lastReconnectAttempt = now;

  WifiStaConfig cfg{};
  if (!wifiStaStoreLoad(&cfg)) {
    return;
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid, cfg.password[0] ? cfg.password : nullptr);
}
