#include "WifiStaStore.h"

#include <EEPROM.h>
#include <cstring>
#include <cstddef>

namespace {

constexpr uint32_t kMagic = 0x57434631;  // "WCF1"
constexpr uint8_t kVersion = 1;

#pragma pack(push, 1)
struct StoredBlob {
  uint32_t magic;
  uint8_t version;
  char ssid[33];
  char password[65];
  uint16_t crc;
};
#pragma pack(pop)

uint16_t crc16Ccitt(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; ++b) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

uint16_t blobCrc(const StoredBlob& c) {
  constexpr size_t n = offsetof(StoredBlob, crc);
  return crc16Ccitt(reinterpret_cast<const uint8_t*>(&c), n);
}

uint16_t g_eepromSize = 0;

}  // namespace

bool wifiStaStoreBegin(uint16_t eepromReserved) {
  g_eepromSize = eepromReserved;
  if (sizeof(StoredBlob) > eepromReserved) {
    return false;
  }
  EEPROM.begin(eepromReserved);
  return true;
}

bool wifiStaStoreLoad(WifiStaConfig* out) {
  if (!out || g_eepromSize == 0) {
    return false;
  }
  StoredBlob b{};
  EEPROM.get(0, b);
  if (b.magic != kMagic || b.version != kVersion) {
    return false;
  }
  if (b.crc != blobCrc(b)) {
    return false;
  }
  b.ssid[sizeof(b.ssid) - 1] = '\0';
  b.password[sizeof(b.password) - 1] = '\0';
  if (b.ssid[0] == '\0') {
    return false;
  }
  memcpy(out->ssid, b.ssid, sizeof(out->ssid));
  memcpy(out->password, b.password, sizeof(out->password));
  return true;
}

bool wifiStaStoreSave(const WifiStaConfig& cfg) {
  if (g_eepromSize == 0) {
    return false;
  }
  StoredBlob b{};
  b.magic = kMagic;
  b.version = kVersion;
  strncpy(b.ssid, cfg.ssid, sizeof(b.ssid) - 1);
  b.ssid[sizeof(b.ssid) - 1] = '\0';
  strncpy(b.password, cfg.password, sizeof(b.password) - 1);
  b.password[sizeof(b.password) - 1] = '\0';
  b.crc = blobCrc(b);
  EEPROM.put(0, b);
  return EEPROM.commit();
}

void wifiStaStoreClear() {
  if (g_eepromSize == 0) {
    return;
  }
  for (uint16_t i = 0; i < g_eepromSize; ++i) {
    EEPROM.write(i, 0xFF);
  }
  EEPROM.commit();
}
