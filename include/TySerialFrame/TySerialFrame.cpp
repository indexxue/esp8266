#include "TySerialFrame.h"

#include <cstring>

static uint8_t tyFrameCrc8(const uint8_t* data, size_t n) {
  uint16_t sum = 0;
  for (size_t i = 0; i < n; ++i) {
    sum = static_cast<uint16_t>(sum + data[i]);
  }
  return static_cast<uint8_t>(sum & 0xFF);
}

enum class RxState : uint8_t { kWaitSof1, kWaitSof2, kCmd, kLenLo, kLenHi, kPayload, kCrc };

static void (*s_onFrame)(uint8_t cmd, const uint8_t* payload, uint16_t len, void* user) = nullptr;
static void* s_user = nullptr;

static RxState s_state = RxState::kWaitSof1;
static uint8_t s_cmd = 0;
static uint16_t s_len = 0;
static uint16_t s_got = 0;
static uint8_t s_payload[kTyFrameMaxPayload]{};

static uint32_t s_seq = 0;
static uint32_t s_ok = 0;
static uint32_t s_crcErr = 0;

static uint8_t s_lastCmd = 0;
static uint16_t s_lastLen = 0;
static uint8_t s_lastPayload[kTyFrameMaxPayload]{};
static uint32_t s_lastSeq = 0;
static bool s_hasLast = false;

static void resetToSof() {
  s_state = RxState::kWaitSof1;
  s_len = 0;
  s_got = 0;
}

void tySerialFrameBegin(void (*onFrame)(uint8_t cmd, const uint8_t* payload, uint16_t len, void* user), void* user) {
  s_onFrame = onFrame;
  s_user = user;
  tySerialFrameResetParser();
}

void tySerialFrameResetParser() {
  resetToSof();
}

uint32_t tySerialFrameOkCount() {
  return s_ok;
}

uint32_t tySerialFrameCrcErrCount() {
  return s_crcErr;
}

bool tySerialFrameCopyLast(uint8_t* outCmd, uint8_t* outPayload, size_t outPayloadCap, uint16_t* outLen, uint32_t* outSeq) {
  if (!s_hasLast || !outCmd || !outPayload || !outLen || !outSeq) {
    return false;
  }
  if (s_lastLen > outPayloadCap) {
    return false;
  }
  *outCmd = s_lastCmd;
  *outLen = s_lastLen;
  *outSeq = s_lastSeq;
  memcpy(outPayload, s_lastPayload, s_lastLen);
  return true;
}

bool tySerialFrameSend(Stream& io, uint8_t cmd, const uint8_t* payload, uint16_t len) {
  if (len > kTyFrameMaxPayload) {
    return false;
  }
  uint8_t blk[3 + kTyFrameMaxPayload];
  blk[0] = cmd;
  blk[1] = static_cast<uint8_t>(len & 0xFF);
  blk[2] = static_cast<uint8_t>((len >> 8) & 0xFF);
  if (len > 0 && payload != nullptr) {
    memcpy(blk + 3, payload, len);
  }
  const uint8_t crc = tyFrameCrc8(blk, static_cast<size_t>(3u) + len);
  io.write(kTyFrameSof1);
  io.write(kTyFrameSof2);
  io.write(cmd);
  io.write(blk[1]);
  io.write(blk[2]);
  if (len > 0 && payload != nullptr) {
    io.write(payload, len);
  }
  io.write(crc);
  return true;
}

void tySerialFramePoll(Stream& io) {
  while (io.available() > 0) {
    const int raw = io.read();
    if (raw < 0) {
      break;
    }
    const uint8_t b = static_cast<uint8_t>(raw);

    switch (s_state) {
      case RxState::kWaitSof1:
        if (b == kTyFrameSof1) {
          s_state = RxState::kWaitSof2;
        }
        break;
      case RxState::kWaitSof2:
        if (b == kTyFrameSof2) {
          s_state = RxState::kCmd;
        } else if (b == kTyFrameSof1) {
          s_state = RxState::kWaitSof2;
        } else {
          s_state = RxState::kWaitSof1;
        }
        break;
      case RxState::kCmd:
        s_cmd = b;
        s_state = RxState::kLenLo;
        break;
      case RxState::kLenLo:
        s_len = b;
        s_state = RxState::kLenHi;
        break;
      case RxState::kLenHi:
        s_len = static_cast<uint16_t>(s_len | (static_cast<uint16_t>(b) << 8));
        if (s_len > kTyFrameMaxPayload) {
          resetToSof();
          break;
        }
        s_got = 0;
        s_state = (s_len == 0) ? RxState::kCrc : RxState::kPayload;
        break;
      case RxState::kPayload:
        s_payload[s_got++] = b;
        if (s_got >= s_len) {
          s_state = RxState::kCrc;
        }
        break;
      case RxState::kCrc: {
        uint8_t blk[3 + kTyFrameMaxPayload];
        blk[0] = s_cmd;
        blk[1] = static_cast<uint8_t>(s_len & 0xFF);
        blk[2] = static_cast<uint8_t>((s_len >> 8) & 0xFF);
        memcpy(blk + 3, s_payload, s_len);
        const uint8_t exp = tyFrameCrc8(blk, static_cast<size_t>(3u) + s_len);
        if (exp != b) {
          ++s_crcErr;
          resetToSof();
          break;
        }
        ++s_ok;
        ++s_seq;
        s_lastCmd = s_cmd;
        s_lastLen = s_len;
        memcpy(s_lastPayload, s_payload, s_len);
        s_lastSeq = s_seq;
        s_hasLast = true;
        if (s_onFrame) {
          s_onFrame(s_cmd, s_payload, s_len, s_user);
        }
        resetToSof();
        break;
      }
    }
  }
}
