/**
 * Example: STA (flash creds + serial CLI) + reconnect + async web + binary UART frames.
 * While offline, Serial runs text provisioning; when online, CLI is off and TySerialFrame owns Serial.
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <AppSta.h>
#include <Web.h>
#include <TyDeviceComm.h>
#include <TySerialFrame.h>
#include <WifiSerialProv.h>

namespace {

constexpr uint16_t kEepromBytes = 256;

void ledTick() {
  static uint32_t t0 = 0;
  const uint32_t now = millis();
  const uint32_t interval = (WiFi.status() == WL_CONNECTED) ? 1000u : 200u;
  if (now - t0 >= interval) {
    t0 = now;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

void logWifiStatusIfChanged() {
  static wl_status_t last = WL_IDLE_STATUS;
  const wl_status_t cur = WiFi.status();
  if (cur == last) {
    return;
  }
  last = cur;
  Serial.printf("[WiFi] status -> %d\n", static_cast<int>(cur));
  if (cur == WL_CONNECTED) {
    Serial.printf("Connected, IP %s\n", WiFi.localIP().toString().c_str());
  }
}

bool s_webStarted = false;

void onTyFrame(uint8_t cmd, const uint8_t* payload, uint16_t len, void* /*user*/) {
  tyDeviceCommOnTyFrame(cmd, payload, len);
  Serial.printf("[TY] frame cmd=%u len=%u\n", static_cast<unsigned>(cmd), static_cast<unsigned>(len));
  if (len > 0 && len <= 32) {
    Serial.print(F("[TY] head: "));
    for (uint16_t i = 0; i < len && i < 16; ++i) {
      Serial.printf("%02X ", payload[i]);
    }
    Serial.println();
  }
}

String hexBytes(const uint8_t* p, uint16_t n) {
  static const char kHex[] = "0123456789ABCDEF";
  String s;
  s.reserve(static_cast<unsigned>(n) * 2u + 8u);
  for (uint16_t i = 0; i < n; ++i) {
    s += kHex[(p[i] >> 4) & 0x0F];
    s += kHex[p[i] & 0x0F];
  }
  return s;
}

void appendLastFrameThenDeviceJson(String& j) {
  uint8_t cmd = 0;
  uint16_t len = 0;
  uint32_t seq = 0;
  uint8_t buf[kTyFrameMaxPayload];

  j += F(",\"last_frame\":");
  if (!tySerialFrameCopyLast(&cmd, buf, sizeof(buf), &len, &seq)) {
    j += F("{\"ok\":false,\"reason\":\"no_frame_yet\"}");
  } else {
    j += F("{\"ok\":true,\"seq\":");
    j += String(seq);
    j += F(",\"cmd\":");
    j += String(cmd);
    j += F(",\"len\":");
    j += String(len);
    j += F(",\"hex\":\"");
    j += hexBytes(buf, len);
    j += F("\"}");
  }
  tyDeviceCommAppendSensorSettingsSyncJson(j);
}

String makeStatusJson() {
  String j;
  j.reserve(1200);
  j += F("{\"wifi_connected\":");
  j += (WiFi.status() == WL_CONNECTED) ? F("true") : F("false");
  j += F(",\"ip\":\"");
  if (WiFi.status() == WL_CONNECTED) {
    j += WiFi.localIP().toString();
  }
  j += F("\",\"rssi\":");
  j += String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  j += F(",\"heap\":");
  j += String(ESP.getFreeHeap());
  j += F(",\"up_ms\":");
  j += String(millis());
  j += F(",\"frames_ok\":");
  j += String(tySerialFrameOkCount());
  j += F(",\"frames_crc_err\":");
  j += String(tySerialFrameCrcErrCount());
  j += F(",\"prov_enabled\":");
  j += wifiSerialProvIsEnabled() ? F("true") : F("false");
  appendLastFrameThenDeviceJson(j);
  j += '}';
  return j;
}

String makeLastFrameJson() {
  uint8_t cmd = 0;
  uint16_t len = 0;
  uint32_t seq = 0;
  uint8_t buf[kTyFrameMaxPayload];
  if (!tySerialFrameCopyLast(&cmd, buf, sizeof(buf), &len, &seq)) {
    return F("{\"ok\":false,\"reason\":\"no_frame_yet\"}");
  }
  String j;
  j.reserve(180 + static_cast<unsigned>(len) * 2u);
  j += F("{\"ok\":true,\"seq\":");
  j += String(seq);
  j += F(",\"cmd\":");
  j += String(cmd);
  j += F(",\"len\":");
  j += String(len);
  j += F(",\"hex\":\"");
  j += hexBytes(buf, len);
  j += F("\"}");
  return j;
}

String handleSettingsPost(const String& body) {
  return tyDeviceCommHandleSettingsPost(body);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();

  if (!wifiStaStoreBegin(kEepromBytes)) {
    Serial.println(F("ERR: wifiStaStoreBegin failed"));
  }

  wifiSerialProvBegin(Serial);
  wifiSerialProvPrintHelp();

  tyDeviceCommBegin();
  tySerialFrameBegin(onTyFrame, nullptr);

  pinMode(LED_BUILTIN, OUTPUT);

  if (!wifiSerialProvAutoloadAndConnect()) {
    Serial.println(F("No valid WiFi config in flash — use SSID, PASS, then SAVE"));
    WiFi.mode(WIFI_STA);
  }
}

void loop() {
  const bool online = (WiFi.status() == WL_CONNECTED);

  if (!online) {
    tyDeviceCommSetStream(nullptr);
    wifiSerialProvSetEnabled(true);
    wifiSerialProvPoll();
  } else {
    tyDeviceCommSetStream(&Serial);
    if (wifiSerialProvIsEnabled()) {
      wifiSerialProvSetEnabled(false);
      while (Serial.available() > 0) {
        (void)Serial.read();
      }
      tySerialFrameResetParser();
    }
    tySerialFramePoll(Serial);

    if (!s_webStarted) {
      webBegin(80, makeStatusJson, makeLastFrameJson, handleSettingsPost);
      s_webStarted = true;
      Serial.printf("\n[HTTP] Open http://%s/ in browser\n\n", WiFi.localIP().toString().c_str());
    }
  }

  tyDeviceCommPoll(millis());
  appStaLoop();
  logWifiStatusIfChanged();
  ledTick();
  yield();
}
