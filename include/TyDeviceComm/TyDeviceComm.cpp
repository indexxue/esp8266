#include "TyDeviceComm.h"

#include <TySerialFrame.h>

#include <cstring>

namespace {

Stream* s_io = nullptr;

bool s_telemOk = false;
uint8_t s_tHum = 0;
uint8_t s_tAudio = 0;
uint8_t s_tWater = 0;
uint8_t s_tRunOn = 0;
uint8_t s_tGear = 0;
bool s_tHasTarget = false;
uint8_t s_tTarget = 0;

bool s_dispValid = false;
uint8_t s_dispTarget = 50;
bool s_dispOn = false;
uint8_t s_dispGear = 0;

bool s_pend = false;
uint8_t s_pendReqId = 0;
uint32_t s_pendDeadlineMs = 0;
uint8_t s_pendTgt = 0;
bool s_pendOn = false;
uint8_t s_pendGear = 0;

String s_lastErr;
uint8_t s_nextReqId = 1;

constexpr uint32_t kSetAckTimeoutMs = 4000;

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

bool jsonFindBool(const String& body, const char* key, bool* outFound, bool* outVal) {
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
  if (i + 3 < n && p[i] == 't' && p[i + 1] == 'r' && p[i + 2] == 'u' && p[i + 3] == 'e') {
    *outFound = true;
    *outVal = true;
    return true;
  }
  if (i + 4 < n && p[i] == 'f' && p[i + 1] == 'a' && p[i + 2] == 'l' && p[i + 3] == 's' && p[i + 4] == 'e') {
    *outFound = true;
    *outVal = false;
    return true;
  }
  return false;
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

uint8_t clampU8(int v, int lo, int hi) {
  if (v < lo) {
    return static_cast<uint8_t>(lo);
  }
  if (v > hi) {
    return static_cast<uint8_t>(hi);
  }
  return static_cast<uint8_t>(v);
}

void applyTelemetryToCache(const uint8_t* p, uint16_t len) {
  if (len < 5) {
    return;
  }
  s_tHum = p[0];
  s_tAudio = p[1];
  s_tWater = p[2];
  s_tRunOn = p[3];
  s_tGear = p[4];
  s_tHasTarget = (len >= 6);
  if (s_tHasTarget) {
    s_tTarget = p[5];
  }
  s_telemOk = true;

  if (!s_pend) {
    if (s_tHasTarget) {
      s_dispTarget = s_tTarget;
    }
    s_dispOn = (s_tRunOn != 0);
    s_dispGear = s_tGear;
    s_dispValid = true;
  }
}

}  // namespace

void tyDeviceCommBegin() {
  s_io = nullptr;
  s_telemOk = false;
  s_dispValid = false;
  s_dispTarget = 50;
  s_dispOn = false;
  s_dispGear = 0;
  s_pend = false;
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
        s_dispTarget = payload[2];
        s_dispOn = (payload[3] != 0);
        s_dispGear = payload[4];
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
    j += F(",\"humidifier_on\":");
    j += (s_tRunOn != 0) ? F("true") : F("false");
    j += F(",\"gear\":");
    j += String(s_tGear);
    if (s_tHasTarget) {
      j += F(",\"target_humidity_pct\":");
      j += String(s_tTarget);
    }
    j += '}';
  }

  j += F(",\"settings\":{");
  if (!s_dispValid) {
    j += F("\"valid\":false");
    j += '}';
  } else {
    j += F("\"valid\":true,\"target_humidity_pct\":");
    j += String(s_dispTarget);
    j += F(",\"humidifier_on\":");
    j += s_dispOn ? F("true") : F("false");
    j += F(",\"gear\":");
    j += String(s_dispGear);
    j += '}';
  }

  j += F(",\"sync\":{");
  j += F("\"pending\":");
  j += s_pend ? F("true") : F("false");
  j += F(",\"req_id\":");
  j += String(s_pend ? static_cast<unsigned>(s_pendReqId) : 0u);
  j += F(",\"deadline_ms\":");
  j += String(s_pend ? s_pendDeadlineMs : 0u);
  j += F(",\"last_error\":\"");
  for (unsigned i = 0; i < static_cast<unsigned>(s_lastErr.length()); ++i) {
    const char c = s_lastErr[static_cast<int>(i)];
    if (c == '"' || c == '\\') {
      j += '\\';
    }
    j += c;
  }
  j += F("\"");
  if (s_pend) {
    j += F(",\"requested\":{\"target_humidity_pct\":");
    j += String(s_pendTgt);
    j += F(",\"humidifier_on\":");
    j += s_pendOn ? F("true") : F("false");
    j += F(",\"gear\":");
    j += String(s_pendGear);
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

  bool fT = false, fO = false, fG = false;
  int vT = 0, vG = 0;
  bool vO = false;
  if (!jsonFindInt(body, "\"target_humidity_pct\"", &fT, &vT)) {
    return F("{\"ok\":false,\"error\":\"bad_json\"}");
  }
  if (!jsonFindBool(body, "\"humidifier_on\"", &fO, &vO)) {
    return F("{\"ok\":false,\"error\":\"bad_json\"}");
  }
  if (!jsonFindInt(body, "\"gear\"", &fG, &vG)) {
    return F("{\"ok\":false,\"error\":\"bad_json\"}");
  }
  if (!fT && !fO && !fG) {
    return F("{\"ok\":false,\"error\":\"empty\"}");
  }

  const uint8_t baseT = s_dispValid ? s_dispTarget : 50;
  const bool baseO = s_dispValid ? s_dispOn : false;
  const uint8_t baseG = s_dispValid ? s_dispGear : 0;

  const uint8_t tgt = clampU8(fT ? vT : static_cast<int>(baseT), 0, 100);
  const bool on = fO ? vO : baseO;
  const uint8_t gear = clampU8(fG ? vG : static_cast<int>(baseG), 0, 255);

  const uint8_t rid = s_nextReqId;
  const uint8_t pl[4] = {rid, tgt, static_cast<uint8_t>(on ? 1u : 0u), gear};
  if (!tySerialFrameSend(*s_io, kTyCmdSetRequest, pl, 4)) {
    return F("{\"ok\":false,\"error\":\"send_failed\"}");
  }

  s_pend = true;
  s_pendReqId = rid;
  s_pendDeadlineMs = millis() + kSetAckTimeoutMs;
  s_pendTgt = tgt;
  s_pendOn = on;
  s_pendGear = gear;
  s_lastErr = "";
  bumpReqId();

  String r;
  r.reserve(96);
  r += F("{\"ok\":true,\"req_id\":");
  r += String(rid);
  r += F(",\"note\":\"wait_for_cmd_0x22\"}");
  return r;
}
