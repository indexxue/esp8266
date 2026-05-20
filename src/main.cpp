/**
 * Example: SoftAP hotspot + async web + binary UART frames.
 * Phone/PC connects to the device AP, then opens http://192.168.4.1/
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <Web.h>
#include <TyDeviceComm.h>
#include <TySerialFrame.h>

namespace {

constexpr char kApSsid[] = "TY-ESP8266";
constexpr char kApPass[] = "12345678";  // min 8 chars; use "" for open AP

/** 有终端连上 SoftAP 时拉低；全部断开时恢复高电平（GPIO12 上电须为高，勿改启动态） */
constexpr uint8_t kApClientGpio = 12;

void onApStationConnected(const WiFiEventSoftAPModeStationConnected& evt) {
  digitalWrite(kApClientGpio, LOW);
  Serial.printf("[AP] station joined aid=%u mac=%02X:%02X:%02X:%02X:%02X:%02X -> GPIO12 LOW\n",
                static_cast<unsigned>(evt.aid), evt.mac[0], evt.mac[1], evt.mac[2], evt.mac[3], evt.mac[4],
                evt.mac[5]);
}

void onApStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) {
  if (WiFi.softAPgetStationNum() == 0) {
    digitalWrite(kApClientGpio, HIGH);
    Serial.printf("[AP] last station left aid=%u mac=%02X:%02X:%02X:%02X:%02X:%02X -> GPIO12 HIGH\n",
                  static_cast<unsigned>(evt.aid), evt.mac[0], evt.mac[1], evt.mac[2], evt.mac[3], evt.mac[4],
                  evt.mac[5]);
  }
}

void ledTick() {
  static uint32_t t0 = 0;
  const uint32_t now = millis();
  if (now - t0 >= 1000u) {
    t0 = now;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

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
  j += F("{\"wifi_mode\":\"ap\",\"ap_ssid\":\"");
  j += kApSsid;
  j += F("\",\"wifi_connected\":true,\"ip\":\"");
  j += WiFi.softAPIP().toString();
  j += F("\",\"ap_clients\":");
  j += String(WiFi.softAPgetStationNum());
  j += F(",\"rssi\":0,\"heap\":");
  j += String(ESP.getFreeHeap());
  j += F(",\"up_ms\":");
  j += String(millis());
  j += F(",\"frames_ok\":");
  j += String(tySerialFrameOkCount());
  j += F(",\"frames_crc_err\":");
  j += String(tySerialFrameCrcErrCount());
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

  tyDeviceCommBegin();
  tySerialFrameBegin(onTyFrame, nullptr);
  tyDeviceCommSetStream(&Serial);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(kApClientGpio, OUTPUT);
  digitalWrite(kApClientGpio, HIGH);

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.onSoftAPModeStationConnected(onApStationConnected);
  WiFi.onSoftAPModeStationDisconnected(onApStationDisconnected);
  const bool apOk = (kApPass[0] != '\0') ? WiFi.softAP(kApSsid, kApPass) : WiFi.softAP(kApSsid);
  if (!apOk) {
    Serial.println(F("ERR: softAP start failed"));
  }
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

  webBegin(80, makeStatusJson, makeLastFrameJson, handleSettingsPost);

  Serial.printf("\n[AP] SSID: %s  IP: %s\n", kApSsid, WiFi.softAPIP().toString().c_str());
  if (kApPass[0] != '\0') {
    Serial.printf("[AP] Password: %s\n", kApPass);
  }
  Serial.println(F("[HTTP] Connect to AP, then open http://192.168.4.1/\n"));
}

void loop() {
  tySerialFramePoll(Serial);
  tyDeviceCommPoll(millis());
  ledTick();
  yield();
}
