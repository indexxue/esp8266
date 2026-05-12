#pragma once

#include <Arduino.h>

/** Periodic STA reconnect from flash after disconnect; uses WifiStaStore. */
void appStaLoop();

bool appStaIsConnected();
