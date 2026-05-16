#pragma once

#include <Arduino.h>

class Stream;

/** 与 STM32 约定的业务 CMD（帧格式见 `TySerialFrame` 与 `doc/esp8266_stm32_link_proto.md`）。 */
constexpr uint8_t kTyCmdSensorReport = 0x20;  // MCU → ESP：遥测
constexpr uint8_t kTyCmdSetRequest = 0x21;    // ESP → MCU：写设置（需 ACK）
constexpr uint8_t kTyCmdSetAck = 0x22;        // MCU → ESP：对 0x21 的应答

/** 0x21 / 0x22 change_mask（§3.3） */
constexpr uint8_t kTyChgHumid = 0x01;
constexpr uint8_t kTyChgLed = 0x02;

/** 网页 GET /api/status 轮询间隔（与 doc §7.3 一致） */
constexpr uint32_t kTyWebPollIntervalMs = 800;
constexpr uint32_t kTyWebPollPendingMs = 400;

constexpr uint32_t kTySetAckTimeoutMs = 4000;

void tyDeviceCommBegin();
void tyDeviceCommSetStream(Stream* io);

void tyDeviceCommOnTyFrame(uint8_t cmd, const uint8_t* payload, uint16_t len);
void tyDeviceCommPoll(uint32_t nowMs);

void tyDeviceCommAppendSensorSettingsSyncJson(String& j);
String tyDeviceCommHandleSettingsPost(const String& body);
