#pragma once

#include <Arduino.h>

/** STA credentials only; separate from EEPROM magic/version/crc layout in .cpp */
struct WifiStaConfig {
  char ssid[33];
  char password[65];
};

bool wifiStaStoreBegin(uint16_t eepromReserved = 256);
bool wifiStaStoreLoad(WifiStaConfig* out);
bool wifiStaStoreSave(const WifiStaConfig& cfg);
void wifiStaStoreClear();
