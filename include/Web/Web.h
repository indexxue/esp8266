#pragma once

#include <Arduino.h>

/** Returns a JSON String (short-lived; copied immediately by send). */
typedef String (*WebJsonFn)();

/** POST /api/settings：body 为 JSON 字符串，返回 JSON 响应。 */
typedef String (*WebSettingsPostFn)(const String& body);

/** Start async HTTP server once; port is usually 80. */
void webBegin(uint16_t port, WebJsonFn statusJson, WebJsonFn lastFrameJson);

void webBegin(uint16_t port, WebJsonFn statusJson, WebJsonFn lastFrameJson, WebSettingsPostFn settingsPost);

bool webIsRunning();
