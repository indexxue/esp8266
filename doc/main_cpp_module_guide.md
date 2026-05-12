# `src/main.cpp` 示例固件：实现逻辑与目录对照

本文说明当前 **应用入口**（`src/main.cpp`）如何把 Wi‑Fi、串口配网、二进制帧协议、异步 Web 串在一起；并标明**代码在哪个文件夹**，方便后续开发或交接。

更通用的工程规范见 [`esp8266_embedded_standard.md`](esp8266_embedded_standard.md)；轻量服务器整体任务清单见 [`esp8266_lightweight_server_tasks.md`](esp8266_lightweight_server_tasks.md)。

---

## 1. 目录与职责一览（先看这张表）

| 路径 | 职责（一句话） |
|------|----------------|
| **`src/main.cpp`** | 上电初始化顺序、`loop()` 里「离线 / 在线」分支、LED、日志、JSON 回调、何时启动 Web。 |
| **`include/WifiStaStore/`** | 把 STA 的 SSID/密码存进 EEPROM（带 magic/CRC），与业务层 `WifiStaConfig` 结构体对应。 |
| **`include/WifiSerialProv/`** | 串口**文本**配网 CLI（SSID、PASS、SAVE 等），内部会调用 `WiFi.begin` 与 `wifiStaStoreSave`。 |
| **`include/AppSta/`** | STA **断线重连**：周期性用 Flash 里凭据再次 `WiFi.begin`（不负责首次配网）。 |
| **`include/TySerialFrame/`** | 与 STM32 约定的 **二进制帧** 解析（非阻塞 `poll`），与文本 CLI **不能同时读同一 `Serial`**。 |
| **`include/Web/`** | 基于 **ESPAsyncWebServer** 的异步 HTTP：首页 HTML + `/api/status`、`/api/lastframe`、`/api/settings`。 |
| **`platformio.ini`** | 板卡/框架、`lib_extra_dirs = include`、第三方库 **`ESPAsyncTCP` / `ESPAsyncWebServer`**、`ASYNC_TCP_SSL_ENABLED=0`。 |

编译产物与依赖缓存一般在 **`.pio/`**（通常不提交版本库）。

---

## 2. 程序在做什么（按时间顺序）

### 2.1 `setup()`：只做一次的事

逻辑顺序与代码一致，便于对照阅读：

1. **`Serial.begin(115200)`**  
   调试输出与配网 CLI 都走 USB 串口（NodeMCU 上即 `Serial`）。

2. **`wifiStaStoreBegin(kEepromBytes)`**  
   初始化 EEPROM 区域大小（本例 256 字节）。失败会打印 `ERR`，实现见 **`include/WifiStaStore/`**。

3. **`wifiSerialProvBegin(Serial)` + `wifiSerialProvPrintHelp()`**  
   把**同一个 `Serial`** 交给配网模块；上电打印命令帮助。实现见 **`include/WifiSerialProv/`**。

4. **`tySerialFrameBegin(onTyFrame, nullptr)`**  
   注册二进制帧收齐后的回调（本例在 `main.cpp` 匿名命名空间里，打印 `cmd/len` 和前几个字节）。解析状态机在 **`include/TySerialFrame/`**。

5. **`pinMode(LED_BUILTIN, OUTPUT)`**  
   板载 LED，用于肉眼判断是否在网（慢闪/快闪）。

6. **`wifiSerialProvAutoloadAndConnect()`**  
   若 Flash 里有合法配置：读入 pending 并 **`WiFi.begin`**。  
   若没有：打印提示，并 **`WiFi.mode(WIFI_STA)`**，等待用户用串口发 `SSID` / `PASS` / `SAVE`。

**要点**：首次连网依赖 **Flash 已有配置** 或 **串口 SAVE**；`AppSta` 不负责「第一次从空配置连上」，只负责**掉线后再连**。

### 2.2 `loop()`：每圈都跑的主状态机

用 **`WiFi.status() == WL_CONNECTED`** 分成「离线」和「在线」两枝，这是理解本示例的核心。

#### 离线（`!online`）

- **`wifiSerialProvSetEnabled(true)`**  
  掉线后重新打开文本 CLI，方便再次改 SSID/密码（无需改代码）。
- **`wifiSerialProvPoll()`**  
  从 `Serial` 读字节、拼行、解析命令。

此阶段 **不调用** `tySerialFramePoll`，避免同一 `Serial` 被两套逻辑抢读。

#### 在线（`online`）

1. **第一次进入在线时的一键切换（`wifiSerialProvIsEnabled()` 为真时）**  
   - **`wifiSerialProvSetEnabled(false)`**：文本 CLI 不再从串口读数据。  
   - **`while (Serial.available()) Serial.read()`**：清空缓冲里可能混着的半行文本，避免干扰帧同步。  
   - **`tySerialFrameResetParser()`**：帧解析状态机回到等 SOF，避免从半帧开始解。

2. **`tySerialFramePoll(Serial)`**  
   独占 `Serial` 做二进制帧解析；收齐一帧会调 **`onTyFrame`**。

3. **第一次在线时启动 Web（`!s_webStarted`）**  
   - **`webBegin(80, makeStatusJson, makeLastFrameJson, handleSettingsPost)`**  
     注册 JSON 与可选的 POST 回调，由 **`include/Web/`** 在收到 HTTP 请求时调用。  
   - **`s_webStarted = true`** 保证只启动一次（库内部也有防重复逻辑）。  
   - 串口打印浏览器访问地址。

#### 无论在线与否都会执行

- **`appStaLoop()`**（**`include/AppSta/`**）  
  若已断线且距上次重连间隔足够，则用 **`wifiStaStoreLoad`** 再发起 `WiFi.begin`。与「串口 SAVE」写入的凭据是同一份。

- **`logWifiStatusIfChanged()`**  
  WiFi 状态变化时打印一次，便于串口监视。

- **`ledTick()`**  
  已连接约 1 s 翻转一次 LED，未连接约 200 ms 翻转（快闪）。

- **`yield()`**  
  让 WiFi / TCP 栈有机会跑；符合工程里「`loop()` 不要长时间占死」的习惯。

---

## 3. `main.cpp` 里各块代码在干什么（按代码块）

以下对应 `src/main.cpp` 中的匿名命名空间与函数，方便新人「从上到下」读源码。

| 符号 / 函数 | 作用 |
|-------------|------|
| **`kEepromBytes`** | 给 `WifiStaStore` 预留的 EEPROM 字节数。 |
| **`ledTick()`** | 非阻塞闪烁；根据是否 `WL_CONNECTED` 选不同周期。 |
| **`logWifiStatusIfChanged()`** | 静态变量记下上次 `WiFi.status()`，变化才打印，避免刷屏。 |
| **`s_webStarted`** | 防止重复 `webBegin`；注意掉线后仍为 `true`（本示例不销毁 WebServer 实例）。 |
| **`onTyFrame(...)`** | 每收到一帧合法二进制数据时调用；示例里只做串口打印，**业务可改成写缓存、置标志位等**。 |
| **`hexBytes()`** | 把字节数组变成十六进制字符串，给 `makeLastFrameJson` 用。 |
| **`makeStatusJson()`** | 拼 `/api/status` 的 JSON：是否联网、IP、RSSI、堆剩余、运行时间、帧统计、`prov_enabled`。 |
| **`makeLastFrameJson()`** | 调 `tySerialFrameCopyLast` 取**最后一帧**，没有则返回 `no_frame_yet`。 |

**扩展建议**：若接口变多，可把 `makeStatusJson` / `makeLastFrameJson` 挪到单独 `.cpp`（仍放在 `src/` 或新 `include/` 子目录），`main.cpp` 只保留 `setup`/`loop` 与模块初始化。

---

## 4. 与 STM32 的串口协议（本仓库约定）

帧格式与常量定义在 **`include/TySerialFrame/TySerialFrame.h`** 注释中：

- **SOF**：`0xA5 0x5A`  
- **CMD**：1 字节  
- **LEN**：小端 16 位，为载荷长度，**不含** CRC  
- **PAYLOAD**：最多 `kTyFrameMaxPayload`（128）字节  
- **CRC8**：`(CMD + LEN_LO + LEN_HI + 全部载荷字节) mod 256`  

若与 STM32 正式协议不一致，应**只改 `TySerialFrame` 内状态机与校验**，尽量保持 `tySerialFramePoll` / `tySerialFrameBegin` / `tySerialFrameCopyLast` 这类 API 稳定，减少 `main.cpp` 改动面。

---

## 5. Web 层如何与 `main` 协作

- **`include/Web/`** 不直接依赖 `TySerialFrame` 类型，只通过 **`WebJsonFn`** / **`WebSettingsPostFn`**（函数指针）向应用层要字符串。  
- 应用层（本例 `main.cpp`）在回调里调用 **`tySerialFrameOkCount`**、**`tySerialFrameCopyLast`** 等，把数据编成 JSON。  
- 首页 HTML 内嵌在 **`Web.cpp`** 的 `PROGMEM` 中，用浏览器轮询 API（轻量、易调试）。若要上 SPIFFS 或大页面，可改 `Web` 或另起模块，见 [`esp8266_lightweight_server_tasks.md`](esp8266_lightweight_server_tasks.md) 第 5 节。

---

## 6. 后续开发常见落点

| 需求 | 建议改哪里 |
|------|------------|
| 改配网命令、加新 CLI | **`include/WifiSerialProv/`** |
| 改 Flash 布局、加密存储 | **`include/WifiStaStore/`**（谨慎，涉及已烧录设备兼容） |
| 改重连间隔、策略 | **`include/AppSta/AppSta.cpp`** |
| 改帧格式 / CRC / 最大长度 | **`include/TySerialFrame/`** |
| 加路由、改页面、加 WebSocket | **`include/Web/`** + `platformio.ini` 依赖 |
| 改上电顺序、在线/离线策略 | **`src/main.cpp`** |

---

## 7. 阅读顺序建议（给接手同事）

1. 本文件（整体数据流与目录）。  
2. **`src/main.cpp`**（`setup` / `loop` 分支）。  
3. **`include/WifiSerialProv/`**（理解串口文本如何写 Flash、如何 `WiFi.begin`）。  
4. **`include/TySerialFrame/`**（理解二进制状态机）。  
5. **`include/Web/`**（理解异步路由与回调）。  
6. [`esp8266_embedded_standard.md`](esp8266_embedded_standard.md)（`loop` 非阻塞、日志约定等）。

按上述顺序，一般可以在半小时内建立「谁读串口、谁起 Web、掉线后发生什么」的心智模型。
