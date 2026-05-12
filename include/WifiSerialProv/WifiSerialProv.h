#pragma once

#include <Arduino.h>

#include <WifiStaStore.h>

/** Bind a Stream for text WiFi provisioning CLI; keeps pending credentials. */
void wifiSerialProvBegin(Stream& io);
void wifiSerialProvPoll();

/** When false, poll() does not read the stream (hand UART to a binary protocol). */
void wifiSerialProvSetEnabled(bool enabled);
bool wifiSerialProvIsEnabled();

void wifiSerialProvPrintHelp();
WifiStaConfig& wifiSerialProvPending();
void wifiSerialProvClearPending();

/** Load valid config from flash into pending and start STA; false if none. */
bool wifiSerialProvAutoloadAndConnect();

/** STA connect from current pending without writing flash. */
void wifiSerialProvApplyStaConnect();
