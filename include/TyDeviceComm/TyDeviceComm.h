#pragma once

#include <Arduino.h>

class Stream;

/** 与 STM32 约定的业务 CMD（帧格式见 `TySerialFrame` 与 `doc/esp8266_stm32_link_proto.md`）。 */
constexpr uint8_t kTyCmdSensorReport = 0x20;  // MCU → ESP：遥测
constexpr uint8_t kTyCmdSetRequest = 0x21;  // ESP → MCU：写设置（需 ACK）
constexpr uint8_t kTyCmdSetAck = 0x22;      // MCU → ESP：对 0x21 的应答

void tyDeviceCommBegin();
void tyDeviceCommSetStream(Stream* io);

/** 收齐一帧后由应用转发进来（与 TySerialFrame 回调同线程）。 */
void tyDeviceCommOnTyFrame(uint8_t cmd, const uint8_t* payload, uint16_t len);

/** 超时清除 pending；在 `loop` 中调用。 */
void tyDeviceCommPoll(uint32_t nowMs);

/**
 * 向 JSON 追加（注意：前面应有其它字段，本函数以 `,` 开头）：
 *   "sensor":{...}   // 来自最近一次合法 0x20 遥测缓存，而非「任意最后一帧」
 *   "settings":{...} // 设备已确认、网页应展示的值（pending 期间冻结为下发前快照）
 *   "sync":{...}     // 是否等待 0x22、req_id、超时时刻、last_error
 */
void tyDeviceCommAppendSensorSettingsSyncJson(String& j);

/** POST /api/settings 的 body（UTF-8 JSON）；返回短 JSON 字符串。 */
String tyDeviceCommHandleSettingsPost(const String& body);
