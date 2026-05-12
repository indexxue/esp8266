#pragma once

#include <Arduino.h>

/**
 * Lightweight binary frame (example; replace per STM32 project):
 *   SOF1 SOF2 | CMD | LEN_LO LEN_HI | PAYLOAD[L] | CRC8
 *   SOF = 0xA5 0x5A; LEN little-endian, excludes CRC;
 *   CRC8 = (CMD + LEN_LO + LEN_HI + each payload byte) mod 256.
 *   Max payload length is kTyFrameMaxPayload.
 */
constexpr uint8_t kTyFrameSof1 = 0xA5;
constexpr uint8_t kTyFrameSof2 = 0x5A;
constexpr uint16_t kTyFrameMaxPayload = 128;

void tySerialFrameBegin(void (*onFrame)(uint8_t cmd, const uint8_t* payload, uint16_t len, void* user), void* user);

/** Non-blocking parse from stream; use only when UART is not shared with text CLI. */
void tySerialFramePoll(Stream& io);

void tySerialFrameResetParser();

uint32_t tySerialFrameOkCount();
uint32_t tySerialFrameCrcErrCount();

/** Copy last good frame (single-threaded loop vs poll is enough on ESP8266). */
bool tySerialFrameCopyLast(uint8_t* outCmd, uint8_t* outPayload, size_t outPayloadCap, uint16_t* outLen, uint32_t* outSeq);

/** Send one frame (blocking write). Same CRC rule as RX. Returns false if LEN too large or io null. */
bool tySerialFrameSend(Stream& io, uint8_t cmd, const uint8_t* payload, uint16_t len);
