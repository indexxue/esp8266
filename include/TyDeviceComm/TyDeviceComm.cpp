#include "TyDeviceComm.h"

#include <TySerialFrame.h>

#include <cstring>

namespace {

Stream* s_io = nullptr;

bool s_telemOk = false;
uint8_t s_tHum = 0;
uint8_t s_tAudio = 0;
uint8_t s_tWater = 0;
uint8_t s_tHumidLevel = 0;
uint8_t s_tLedMode = 0;
bool s_tOverheat = false;

bool s_dispValid = false;
uint8_t s_dispHumidLevel = 0;
uint8_t s_dispLedMode = 0;

bool s_pend = false;
uint8_t s_pendReqId = 0;
uint8_t s_pendMask = 0;
uint32_t s_pendDeadlineMs = 0;
uint8_t s_pendHumidLevel = 0;
uint8_t s_pendLedMode = 0;

String s_lastErr;
uint8_t s_nextReqId = 1;

void bumpReqId() {
  uint8_t n = static_cast<uint8_t>(s_nextReqId + 1u);
  if (n == 0) {
    n = 1;
  }
  s_nextReqId = n;
}

static void skipWs(const char* p, size_t n, size_t* i) {
  while (*i < n && (p[*i] == ' ' || p[*i] == '\t' || p[*i] == '\r' || p[*i] == '\n')) {
    ++(*i);
  }
}

bool jsonFindInt(const String& body, const char* key, bool* outFound, int* outVal) {
  *outFound = false;
  const int pos = body.indexOf(key);
  if (pos < 0) {
    return true;
  }
  size_t i = static_cast<size_t>(pos) + strlen(key);
  const char* p = body.c_str();
  const size_t n = body.length();
  skipWs(p, n, &i);
  if (i >= n || p[i] != ':') {
    return false;
  }
  ++i;
  skipWs(p, n, &i);
  bool neg = false;
  if (i < n && p[i] == '-') {
    neg = true;
    ++i;
  }
  if (i >= n || p[i] < '0' || p[i] > '9') {
    return false;
  }
  long v = 0;
  while (i < n && p[i] >= '0' && p[i] <= '9') {
    v = v * 10 + (p[i] - '0');
    ++i;
  }
  if (neg) {
    v = -v;
  }
  *outFound = true;
  *outVal = static_cast<int>(v);
  return true;
}

uint8_t clampLevel03(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 3) {
    return 3;
  }
  return static_cast<uint8_t>(v);
}

void applyTelemetryToCache(const uint8_t* p, uint16_t len) {
  if (len != 6) {
    return;
  }
  s_tHum = p[0];
  s_tAudio = p[1];
  s_tWater = p[2];
  s_tHumidLevel = clampLevel03(p[3]);
  s_tLedMode = clampLevel03(p[4]);
  s_tOverheat = ((p[5] & 0x01) != 0);
  s_telemOk = true;

  if (!s_pend) {
    s_dispHumidLevel = s_tHumidLevel;
    s_dispLedMode = s_tLedMode;
    s_dispValid = true;
  }
}

void appendJsonEscaped(String& j, const String& s) {
  for (unsigned i = 0; i < static_cast<unsigned>(s.length()); ++i) {
    const char c = s[static_cast<int>(i)];
    if (c == '"' || c == '\\') {
      j += '\\';
    }
    j += c;
  }
}

}  // namespace

void tyDeviceCommBegin() {
  s_io = nullptr;
  s_telemOk = false;
  s_dispValid = false;
  s_dispHumidLevel = 0;
  s_dispLedMode = 0;
  s_pend = false;
  s_pendMask = 0;
  s_lastErr = "";
  s_nextReqId = 1;
}

void tyDeviceCommSetStream(Stream* io) {
  s_io = io;
}

void tyDeviceCommOnTyFrame(uint8_t cmd, const uint8_t* payload, uint16_t len) {
  if (cmd == kTyCmdSensorReport) {
    applyTelemetryToCache(payload, len);
    return;
  }
  if (cmd == kTyCmdSetAck && len >= 5) {
    const uint8_t rid = payload[0];
    const uint8_t st = payload[1];
    if (s_pend && rid == s_pendReqId) {
      if (st == 0) {
        s_pend = false;
        s_dispHumidLevel = clampLevel03(payload[3]);
        s_dispLedMode = clampLevel03(payload[4]);
        s_dispValid = true;
        s_lastErr = "";
      } else {
        s_pend = false;
        s_lastErr = "mcu_rejected:";
        s_lastErr += String(static_cast<unsigned>(st));
      }
    }
  }
}

void tyDeviceCommPoll(uint32_t nowMs) {
  if (!s_pend) {
    return;
  }
  if ((int32_t)(nowMs - s_pendDeadlineMs) < 0) {
    return;
  }
  s_pend = false;
  s_lastErr = "timeout";
}

void tyDeviceCommAppendSensorSettingsSyncJson(String& j) {
  j += F(",\"sensor\":{");
  if (!s_telemOk) {
    j += F("\"valid\":false,\"reason\":\"no_telemetry_yet\"}");
  } else {
    j += F("\"valid\":true,\"humidity_pct\":");
    j += String(s_tHum);
    j += F(",\"audio_level\":");
    j += String(s_tAudio);
    j += F(",\"volume_level\":");
    j += String(s_tAudio);
    j += F(",\"water_level_pct\":");
    j += String(s_tWater);
    j += F(",\"humidifier_level\":");
    j += String(s_tHumidLevel);
    j += F(",\"led_strip_mode\":");
    j += String(s_tLedMode);
    j += F(",\"overheat\":");
    j += s_tOverheat ? F("true") : F("false");
    j += '}';
  }

  j += F(",\"settings\":{");
  if (!s_dispValid) {
    j += F("\"valid\":false");
    j += '}';
  } else {
    j += F("\"valid\":true,\"humidifier_level\":");
    j += String(s_dispHumidLevel);
    j += F(",\"led_strip_mode\":");
    j += String(s_dispLedMode);
    j += '}';
  }

  j += F(",\"sync\":{");
  j += F("\"pending\":");
  j += s_pend ? F("true") : F("false");
  j += F(",\"req_id\":");
  j += String(s_pend ? static_cast<unsigned>(s_pendReqId) : 0u);
  j += F(",\"change_mask\":");
  j += String(s_pend ? static_cast<unsigned>(s_pendMask) : 0u);
  j += F(",\"deadline_ms\":");
  j += String(s_pend ? s_pendDeadlineMs : 0u);
  j += F(",\"last_error\":\"");
  appendJsonEscaped(j, s_lastErr);
  j += F("\",\"poll_interval_ms\":");
  j += String(kTyWebPollIntervalMs);
  j += F(",\"poll_interval_pending_ms\":");
  j += String(kTyWebPollPendingMs);
  if (s_pend) {
    j += F(",\"requested\":{");
    bool first = true;
    if ((s_pendMask & kTyChgHumid) != 0) {
      j += F("\"humidifier_level\":");
      j += String(s_pendHumidLevel);
      first = false;
    }
    if ((s_pendMask & kTyChgLed) != 0) {
      if (!first) {
        j += ',';
      }
      j += F("\"led_strip_mode\":");
      j += String(s_pendLedMode);
    }
    j += '}';
  }
  j += '}';
}

String tyDeviceCommHandleSettingsPost(const String& body) {
  if (s_io == nullptr) {
    return F("{\"ok\":false,\"error\":\"uart_not_ready\"}");
  }
  if (s_pend) {
    return F("{\"ok\":false,\"error\":\"pending\"}");
  }

  bool fH = false;
  bool fL = false;
  int vH = 0;
  int vL = 0;
  if (!jsonFindInt(body, "\"humidifier_level\"", &fH, &vH)) {
    return F("{\"ok\":false,\"error\":\"bad_json\"}");
  }
  if (!jsonFindInt(body, "\"led_strip_mode\"", &fL, &vL)) {
    return F("{\"ok\":false,\"error\":\"bad_json\"}");
  }
  if (!fH && !fL) {
    return F("{\"ok\":false,\"error\":\"empty\"}");
  }

  uint8_t mask = 0;
  uint8_t hum = 0;
  uint8_t led = 0;
  if (fH) {
    mask |= kTyChgHumid;
    hum = clampLevel03(vH);
  }
  if (fL) {
    mask |= kTyChgLed;
    led = clampLevel03(vL);
  }

  const uint8_t rid = s_nextReqId;
  const uint8_t pl[4] = {rid, mask, hum, led};
  if (!tySerialFrameSend(*s_io, kTyCmdSetRequest, pl, 4)) {
    return F("{\"ok\":false,\"error\":\"send_failed\"}");
  }

  s_pend = true;
  s_pendReqId = rid;
  s_pendMask = mask;
  s_pendDeadlineMs = millis() + kTySetAckTimeoutMs;
  s_pendHumidLevel = hum;
  s_pendLedMode = led;
  s_lastErr = "";
  bumpReqId();

  String r;
  r.reserve(128);
  r += F("{\"ok\":true,\"req_id\":");
  r += String(rid);
  r += F(",\"change_mask\":");
  r += String(static_cast<unsigned>(mask));
  r += F(",\"note\":\"wait_for_cmd_0x22\"}");
  return r;
}
