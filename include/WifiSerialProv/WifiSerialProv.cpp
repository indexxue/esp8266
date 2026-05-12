#include "WifiSerialProv.h"

#include <ESP8266WiFi.h>
#include <cstring>
#include <cctype>

namespace {

Stream* s_io = nullptr;
WifiStaConfig s_pending{};
char s_line[200];
size_t s_lineLen = 0;
bool s_enabled = true;

void trimTrailing(char* s) {
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) {
    s[--n] = '\0';
  }
}

const char* argAfterFirstSpace(const char* line) {
  const char* p = strchr(line, ' ');
  if (!p) {
    return nullptr;
  }
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  return (*p == '\0') ? nullptr : p;
}

void printHelp() {
  if (!s_io) {
    return;
  }
  s_io->println(F("\n--- WiFi serial provisioning ---"));
  s_io->println(F("HELP          Show this help"));
  s_io->println(F("STATUS        Link state / IP / RSSI"));
  s_io->println(F("SCAN          Scan for APs"));
  s_io->println(F("SSID <name>   Set pending SSID (<=32 chars)"));
  s_io->println(F("PASS <secret> Set pending password (<=64 chars)"));
  s_io->println(F("SHOW          Pending vs stored summary"));
  s_io->println(F("SAVE          Write EEPROM and reconnect"));
  s_io->println(F("CLEAR         Erase stored WiFi config"));
  s_io->println(F("REBOOT        Restart module"));
}

void cmdStatus() {
  if (!s_io) {
    return;
  }
  const wl_status_t st = WiFi.status();
  s_io->printf("WiFi.status=%d (%s)\n", static_cast<int>(st), WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISC");
  if (WiFi.status() == WL_CONNECTED) {
    s_io->printf("SSID: %s\n", WiFi.SSID().c_str());
    s_io->printf("IP:   %s\n", WiFi.localIP().toString().c_str());
    s_io->printf("RSSI: %d dBm\n", WiFi.RSSI());
    s_io->printf("MAC:  %s\n", WiFi.macAddress().c_str());
  }
}

void cmdScan() {
  if (!s_io) {
    return;
  }
  s_io->println(F("Scanning..."));
  const int n = WiFi.scanNetworks(false, true);
  if (n < 0) {
    s_io->println(F("scan failed"));
    return;
  }
  s_io->printf("%d networks:\n", n);
  for (int i = 0; i < n; ++i) {
    s_io->printf("  %2d: %-32s  RSSI:%4d  CH:%2d  %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                  (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "open" : "enc");
  }
  WiFi.scanDelete();
}

void cmdShow() {
  if (!s_io) {
    return;
  }
  s_io->println(F("[Pending — written to flash after SAVE]"));
  s_io->printf("  SSID: %s\n", s_pending.ssid[0] ? s_pending.ssid : "(empty)");
  s_io->print(F("  PASS: "));
  if (s_pending.password[0]) {
    s_io->println(F("********"));
  } else {
    s_io->println(F("(empty)"));
  }
  s_io->println(F("[Stored in Flash]"));
  WifiStaConfig flash{};
  if (wifiStaStoreLoad(&flash)) {
    s_io->printf("  SSID: %s\n", flash.ssid);
    s_io->println(F("  PASS: ********"));
  } else {
    s_io->println(F("  (no valid config)"));
  }
}

bool copyField(char* dest, size_t destSz, const char* src) {
  if (!s_io) {
    return false;
  }
  if (!src) {
    s_io->println(F("ERR: missing argument"));
    return false;
  }
  const size_t len = strlen(src);
  if (len >= destSz) {
    s_io->printf("ERR: too long (max %u bytes)\n", static_cast<unsigned>(destSz - 1));
    return false;
  }
  strncpy(dest, src, destSz - 1);
  dest[destSz - 1] = '\0';
  trimTrailing(dest);
  return true;
}

void applyStaConnect() {
  if (!s_io) {
    return;
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  if (s_pending.ssid[0] == '\0') {
    s_io->println(F("WARN: SSID empty, skip connect"));
    return;
  }
  s_io->printf("Connecting to \"%s\" ...\n", s_pending.ssid);
  WiFi.begin(s_pending.ssid, s_pending.password[0] ? s_pending.password : nullptr);
}

void handleLine(const char* rawLine) {
  if (!s_io) {
    return;
  }
  while (*rawLine == ' ' || *rawLine == '\t') {
    ++rawLine;
  }
  if (*rawLine == '\0') {
    return;
  }

  char line[sizeof(s_line)];
  strncpy(line, rawLine, sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';

  char cmd[16]{};
  const char* sp = strchr(line, ' ');
  if (sp) {
    const size_t clen = static_cast<size_t>(sp - line);
    if (clen >= sizeof(cmd)) {
      s_io->println(F("ERR: unknown command"));
      return;
    }
    memcpy(cmd, line, clen);
    cmd[clen] = '\0';
  } else {
    strncpy(cmd, line, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';
  }

  for (size_t i = 0; cmd[i]; ++i) {
    cmd[i] = static_cast<char>(tolower(static_cast<unsigned char>(cmd[i])));
  }

  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
    printHelp();
  } else if (!strcmp(cmd, "status")) {
    cmdStatus();
  } else if (!strcmp(cmd, "scan")) {
    cmdScan();
  } else if (!strcmp(cmd, "ssid")) {
    const char* arg = argAfterFirstSpace(line);
    if (copyField(s_pending.ssid, sizeof(s_pending.ssid), arg)) {
      s_io->println(F("OK: pending SSID set (use SAVE to store)"));
    }
  } else if (!strcmp(cmd, "pass") || !strcmp(cmd, "password")) {
    const char* arg = argAfterFirstSpace(line);
    if (copyField(s_pending.password, sizeof(s_pending.password), arg)) {
      s_io->println(F("OK: pending PASS set (use SAVE to store)"));
    }
  } else if (!strcmp(cmd, "show")) {
    cmdShow();
  } else if (!strcmp(cmd, "save")) {
    if (s_pending.ssid[0] == '\0') {
      s_io->println(F("ERR: SSID empty"));
      return;
    }
    if (!wifiStaStoreSave(s_pending)) {
      s_io->println(F("ERR: EEPROM.commit failed"));
      return;
    }
    s_io->println(F("OK: saved to flash"));
    applyStaConnect();
  } else if (!strcmp(cmd, "clear")) {
    wifiStaStoreClear();
    memset(&s_pending, 0, sizeof(s_pending));
    WiFi.disconnect(true);
    s_io->println(F("OK: flash config cleared"));
  } else if (!strcmp(cmd, "reboot") || !strcmp(cmd, "reset")) {
    s_io->println(F("Rebooting..."));
    delay(200);
    ESP.restart();
  } else {
    s_io->println(F("ERR: unknown command (type HELP)"));
  }
}

}  // namespace

void wifiSerialProvBegin(Stream& io) {
  s_io = &io;
  s_lineLen = 0;
}

void wifiSerialProvSetEnabled(bool enabled) {
  s_enabled = enabled;
}

bool wifiSerialProvIsEnabled() {
  return s_enabled;
}

void wifiSerialProvPoll() {
  if (!s_io || !s_enabled) {
    return;
  }
  while (s_io->available() > 0) {
    const int ch = s_io->read();
    if (ch < 0) {
      break;
    }
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      s_line[s_lineLen] = '\0';
      s_lineLen = 0;
      handleLine(s_line);
      continue;
    }
    if (s_lineLen < sizeof(s_line) - 1) {
      s_line[s_lineLen++] = static_cast<char>(ch);
    } else {
      s_lineLen = 0;
      s_io->println(F("ERR: line too long, discarded"));
    }
  }
}

void wifiSerialProvPrintHelp() {
  printHelp();
}

WifiStaConfig& wifiSerialProvPending() {
  return s_pending;
}

void wifiSerialProvClearPending() {
  memset(&s_pending, 0, sizeof(s_pending));
}

bool wifiSerialProvAutoloadAndConnect() {
  if (!wifiStaStoreLoad(&s_pending)) {
    return false;
  }
  if (s_io) {
    s_io->printf("Loaded WiFi config from flash, SSID=\"%s\"\n", s_pending.ssid);
  }
  applyStaConnect();
  return true;
}

void wifiSerialProvApplyStaConnect() {
  applyStaConnect();
}
