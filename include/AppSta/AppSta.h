#pragma once

#include <Arduino.h>

/** Periodic STA reconnect from flash after disconnect; uses WifiStaStore. */
void appStaLoop();

bool appStaIsConnected();

/**
 * Drop STA association (works even when WL_CONNECTED). Auto-reconnect from flash
 * is deferred by the usual reconnect interval so Serial provisioning can run first.
 */
void appStaUserDisconnect();
