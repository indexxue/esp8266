# ESP8266 开发规范与工程说明

**适用范围**：本仓库在 **ESP8266** 上使用 **PlatformIO**、**Arduino 框架**（`espressif8266` 平台）进行固件开发。  
**应用入口**：`src/`（`setup()` / `loop()`）。  
**烧录、串口监视与常见故障**：见 [`README.md`](README.md)（本目录）。

本文档结构：**开发规范** → **编码规范** → **文件与目录创建规范** → **编译与工程说明**。

---

# 第一部分：开发规范

## 1.1 分层与依赖方向

本仓库以 **PlatformIO 工程 + `lib/` 私有库** 为边界组织代码。分层与 ESP-IDF 组件不同，但建议保持相同的**依赖方向**思想，便于后续迁移或多人协作：

- **`src/`（应用层）**：入口与业务编排（`setup` / `loop`、模块初始化顺序、主状态机）。  
  允许依赖：`lib/` 下各库、Arduino / ESP8266 核心库（如 `ESP8266WiFi`）、第三方库。

- **`lib/` 中的「通用」模块**：与具体 PCB 丝印无关的可复用逻辑（如统一日志格式、纯算法、与引脚无关的协议解析）。  
  尽量不反向依赖仅存在于 `src/` 的全局状态；需要硬件时通过**注入接口**或**显式初始化参数**传入，而不是直接 `#include` 应用层头文件。

- **`lib/` 中的「板级 / BSP」封装**：GPIO、UART、I2C、定时器等与具体引脚/外设相关的薄封装。  
  不依赖应用层；尽量不依赖「产品业务」库，避免底层被上层牵着走。

- **`lib/` 中的「器件 / 功能块」（cbb）**：具体芯片、传感器、显示控制器等驱动。  
  可依赖 BSP 提供的抽象或直接使用 Arduino 外设 API；**不得**依赖应用层专有头文件。

**禁止**：BSP 或 cbb 层 `#include` 仅定义在 `src/` 内的头文件；尽量避免 `lib/A` ↔ `lib/B` 的环状依赖。新增模块前先确定所属层级，再决定目录与对外 API。

> 小型工程可暂不拆分子目录，但**命名与头文件暴露的 API** 应能看出层级（例如 `WifiStaStore` 偏通用存储，`board_led` 偏 BSP）。

## 1.2 初始化与错误处理

1. **启动顺序**：建议显式分层初始化（例如串口 → 存储 → 网络相关模块 → 板载指示），顺序写在 `setup()` 中并加简短注释。  
2. **失败路径**：初始化失败应通过 `Serial`（或统一日志）输出明确信息，并进入安全状态（重试、停机提示、避免继续访问未就绪子系统），**禁止**静默跳过导致后续未定义行为。  
3. **返回值**：对返回 `bool` / 状态码的封装 API 进行检查；Arduino 部分 API 无返回值时，用后续查询 API 或状态机确认结果。

## 1.3 调度、WiFi 栈与「勿长时间占用 CPU」

ESP8266 在 Arduino 框架下**没有完整桌面 OS 的多任务模型**；`loop()` 与 **TCP/IP、WiFi 后台**共享时间片。约定：

1. **应用层主模型（本仓库）**：业务与周期逻辑以 **`setup()` / `loop()`** 为主线编排；**不在草图层引入 FreeRTOS 用户任务**（如 `xTaskCreate`），也**不采用** `Ticker` + `schedule_function` 等方式把主流程拆成「伪 RTOS 任务」。需要多段周期工作时，在 `loop()` 内用 **`millis()` 非阻塞状态机**或保持简短轮询并适时 `delay`/`yield` 即可。若将来必须抢占式多任务，应单独评估 **ESP8266 RTOS SDK** 或 **ESP32** 等目标，而不是在现 Arduino 草图里硬套 FreeRTOS。  
2. **`loop()` 保持轻快**：避免长时间 `delay()`、忙等、大块同步网络读写不加超时；需要周期任务时优先拆成小步或使用 `millis()` 非阻塞状态机。  
3. **必要时 `yield()`**：在少数长循环或密集计算中插入 `yield()`，避免看门狗复位与网络栈饥饿（仍以「缩短临界区」为首选）。  
4. **`delay()`**：仅用于上电短时稳定等可接受场景；主循环中优先用时间戳轮询；若使用短 `delay()` 让出 CPU，应控制总周期，避免拖慢串口响应。  
5. **中断服务程序（ISR）**：只做最短工作（置标志、写队列）；不调用占用堆或耗时的 Arduino / WiFi API；日志尽量不在 ISR 中大量输出。

## 1.4 日志、配置与版本管理

1. 调试阶段使用 **`Serial`** 或封装函数；需要分模块过滤时，可用统一前缀（如 `[WiFi]`、`[PROV]`）。  
2. 与编译相关的选项以 **`platformio.ini`**、`build_flags` 为准；提交前确认未误提交本机绝对路径、密钥或无关差异。  
3. **提交前**：应能通过本仓库推荐方式完成编译；**尽量不引入新的编译告警**；提交说明用完整句子写清动机与影响范围。  
4. 涉及引脚、波特率、分区/Flash 布局、依赖库版本变更时，在提交说明或 `doc/` 中注明，便于他人复现。

## 1.5 与专项文档的关系

- 编译、烧录、监视步骤：[`README.md`](README.md)（本目录）  
- 本文档：开发 / 编码 / 目录约定 / 工程说明总览  

---

# 第二部分：编码规范

## 2.1 总体原则

1. **可读性优于炫技**：复杂条件拆分，层次清晰。  
2. **单一职责**：每个 `.cpp` / `.c` 文件围绕一个外设、器件或清晰子功能；函数长度建议控制在约 50～80 行以内，过长则拆分。  
3. **防御性编程**：校验指针、长度、缓冲区边界；串口协议与传感器数据需校验范围与状态机合法性。  
4. **避免魔法数字**：有业务含义的常量用 `enum class`、`constexpr`、`static const` 或命名宏，并注明单位（ms、Hz、raw 等）。  
5. **可移植与宽度**：协议与寄存器位宽优先使用 `<cstdint>` / `stdint.h`（`uint8_t`、`uint32_t` 等）；跨 Flash 或网络的结构体需在注释中说明对齐、**字节序**与版本兼容策略。

## 2.2 命名约定（C++ / Arduino 风格）

| 类别 | 规则 | 示例 |
|------|------|------|
| 类型、类 | `PascalCase` | `WifiSerialProv` |
| 函数、方法 | `camelCase` 或小写+下划线（与现有模块一致即可） | `wifiStaStoreBegin`、`wifiSerialProvPoll` |
| 局部变量 | `camelCase` 或 `snake_case`，团队统一即可 | `retryCount`、`line_level` |
| 文件作用域静态 | 前缀 `s_`（可选） | `s_isInited` |
| 全局变量 | 尽量少用；若需要，前缀 `g_` | `g_logLevel` |
| 常量 | `kPascalCase`（Google 风格）或全大写宏 | `kEepromBytes`、`MAX_RETRY` |
| `enum class` | 标签清晰，枚举值带模块前缀更佳 | `ProvState::Idle` |

对外 API：**模块名 + 动词** 或 **动词 + 对象**；布尔语义可用 `is` / `has` 前缀。

## 2.3 错误与返回值

1. 封装模块优先返回 **`bool`** 或 **小型 `enum class`**，失败时在实现内输出日志（若已初始化串口）。  
2. 避免在量产路径滥用「失败即重启」；应返回错误、记录日志或降级到安全模式。  
3. 自定义错误语义在头文件或本文档中简要说明。

## 2.4 日志与字符串 RAM 占用

1. 固定提示字符串尽量使用 **`F()` 宏**放入 Flash，减轻 RAM 压力，例如：`Serial.println(F("ERR: ..."))`。  
2. 热路径避免高频 `Serial.print`；ISR 中避免大量输出。  
3. 需要更细调试时可使用 ESP8266 生态中的 **`DEBUG_ESP_*`** 等机制（在 `build_flags` 中打开对应宏），与团队约定一致即可。

## 2.5 头文件与包含顺序

1. **对应 `.cpp` 的第一行**包含本模块 `.h`（若有），保证头文件自完备。  
2. 其次为 **Arduino / ESP8266** 头文件，再为 **项目 `lib/`** 头文件，最后 **C++ 标准库 / C 标准库**。  
3. 头文件保护：使用 `#pragma once` 或 `#ifndef` 守卫，仓库内统一一种。  
4. **最小可见性**：对外 API 放入 `.h`；仅本编译单元使用的 `static` / 匿名命名空间函数与常量放在 `.cpp`。  
5. 避免在头文件中定义非 `inline` 的对象或大块内联实现。

## 2.6 宏与安全用法

1. 带参宏对参数加括号；多语句宏使用 `do { ... } while (0)`。  
2. 与寄存器或硬件位相关的宏在注释中标明数据手册章节或符号名。  
3. 避免用复杂宏替代函数导致无法单步调试。

## 2.7 中断与 IRAM（ESP8266）

1. **ISR 尽量短**；与 `loop()` 协作可用 `volatile` 标志、`std::atomic`（若可用）或队列，避免在 ISR 里调用非 `*_ISR` 安全 API。  
2. 必须在 ISR 或临界 timing 中执行的代码/常量，遵循 Espressif / Arduino-ESP8266 文档使用 **`ICACHE_RAM_ATTR`** 等属性，避免执行路径不当导致异常或崩溃。  
3. **临界区**：访问与 ISR 共享的数据时，使用 `noInterrupts()` / `interrupts()` 或框架提供的互斥手段，并尽量缩短关中断时间。

## 2.8 内存与资源（ESP8266 重点）

1. **堆很小且易碎片**：动态 `new` / `malloc` 尽量集中在启动阶段；运行中频繁分配释放需谨慎，并处理失败路径。  
2. **栈深度**：递归与深层调用链在 ESP8266 上风险高；大数组尽量不放栈上，改为 `static` 或明确生命周期的全局/单例缓冲。  
3. **`String` 类**：频繁拼接可能导致堆碎片；关键路径优先定长缓冲、`snprintf`、`Stream` 直接输出。  
4. **`deinit` 路径**（若实现）：释放定时器回调、注销中断、关闭连接，与 `init` 对称。

## 2.9 断言

开发阶段可使用 **`assert`** 尽早暴露非法状态；发布策略由团队统一（保留、关闭或改为日志 + 安全停机），**禁止**依赖「断言被优化掉」掩盖逻辑错误。

## 2.10 编译告警

新增代码应在当前工程默认选项下 **不引入新告警**；对接第三方库产生告警时，应局部、有注释地处理并评审。

---

# 第三部分：文件与目录创建规范

## 3.1 应把新代码放在哪里

| 内容 | 放置位置 | 说明 |
|------|----------|------|
| 应用主流程、`setup` / `loop` | `src/` | 保持入口清晰；复杂逻辑可拆成 `src/` 内多文件 |
| 可复用功能模块（配网、存储、协议等） | `lib/<库名>/` | 每个库独立目录；对外头文件与源文件成对 |
| 工程级公共宏、类型（可选） | `include/` | 在 `platformio.ini` 中配置 `build_flags` 的 `-I` 或默认 include 路径 |
| 单元测试（若启用） | `test/` | 见 PlatformIO 测试文档 |

PlatformIO 会通过 **Library Dependency Finder (LDF)** 扫描 `#include` 关联 `lib/` 下库；若自动发现失败，可在 `lib/<name>/library.json` 中补充元数据或调整 `lib_ldf_mode`。

## 3.2 新建或扩展 `lib/` 私有库的步骤

1. 在 `lib/<LibraryName>/` 下创建目录，放入 `.h` / `.cpp`（或 `.c`），**文件名与模块职责一致**。  
2. 若需特殊编译选项、依赖声明或 LDF 提示，增加 **`library.json`**（参考 [PlatformIO library.json](https://docs.platformio.org/en/latest/manifests/library-json/index.html)）。  
3. 在 `src/` 或上层库中 `#include <YourHeader.h>`，全量编译验证链接与头文件路径。  
4. 完成后在工程根执行 **`py -3.11 -m platformio run`**（或已配置的 `pio` 命令）做全量验证。

## 3.3 文件命名

1. 源/头文件推荐 **PascalCase**（与现有 `WifiStaStore` 一致）或 **小写 + 下划线**，**仓库内统一一种主风格**。  
2. 避免单字母文件名或与标准库易混淆的名称。  
3. 同一模块 **`.cpp` / `.h` 成对出现**；公共头文件中不堆积无关模块声明。

## 3.4 禁止与不推荐

1. **禁止**在 BSP/器件库中直接 `#include` 仅在 `src/` 定义的应用私有头文件。  
2. **不推荐**在 `src/main.cpp` 中堆放大段寄存器位操作；应下沉到 `lib/` 并封装可读 API。  
3. **禁止**将 `.pio/build/`、含密钥或本机绝对路径的本地配置提交到 Git（以团队 `.gitignore` 为准）。

---

# 第四部分：编译与工程说明

## 4.1 环境与首次准备

1. **操作系统**：以 Windows 10/11 为主；安装 **Python**（本仓库文档示例为 `py -3.11`）与 **PlatformIO Core**（CLI 或 VS Code / Cursor 的 PlatformIO 扩展）。  
2. **进入工程根目录**（路径按本机修改）：

   ```powershell
   Set-Location "d:\desktopsrc\TY\ESP8266"
   ```

3. 首次编译时 PlatformIO 会按需下载 **平台与工具链**（需网络）。

## 4.2 工程布局（摘要）

- 工程配置：根目录 **`platformio.ini`**（当前示例：`[env:nodemcuv2]`，`platform = espressif8266`，`framework = arduino`）。  
- 应用源码：`src/`。  
- 私有库：`lib/`。  
- 构建输出：`.pio/build/<环境名>/`（勿提交）。

## 4.3 板型与目标

板型由 `platformio.ini` 的 `board = ...` 指定（如 `nodemcuv2`）。更换硬件时改为 [PlatformIO 支持的板型 ID](https://docs.platformio.org/en/latest/platforms/espressif8266.html#boards)，并核对 Flash 大小、引脚是否与代码一致。

## 4.4 编译命令

```powershell
py -3.11 -m platformio run
```

在已安装 PlatformIO 扩展的 IDE 中，也可使用 **Build** 按钮执行相同操作。

## 4.5 配置与调优

- 全局编译宏、标准级别、优化选项：在 **`platformio.ini`** 的 `build_flags` 中配置。  
- 上传端口、波特率、监视器波特率：`upload_port`、`upload_speed`、`monitor_speed` 等。  
- 修改后建议 **Clean** 再全量编译，避免陈旧对象文件干扰。

## 4.6 诊断与常见问题

| 目的 | 命令示例 |
|------|----------|
| 编译 | `py -3.11 -m platformio run` |
| 上传 | `py -3.11 -m platformio run -t upload` |
| 串口列表 | `py -3.11 -m platformio device list` |
| 监视器 | `py -3.11 -m platformio device monitor --baud 115200` |

常见问题：未安装 USB 转串口驱动、`COM` 端口占用、上传速率过高、`board` 与实物不一致、**堆栈不足**导致异常重启等。更详细的步骤与排错见 [`README.md`](README.md)。

---

# 附录：文档索引

| 主题 | 文档 |
|------|------|
| 开发 / 编码 / 文件 / 编译（本文） | `doc/esp8266_embedded_standard.md` |
| 烧录、监视、常见问题 | `doc/README.md` |
| PlatformIO 官方 | [PlatformIO ESP8266](https://docs.platformio.org/en/latest/platforms/espressif8266.html) |

---

# 修订记录

| 日期 | 说明 |
|------|------|
| 2026-05-12 | 初版：基于团队 ESP32-S3（ESP-IDF）规范改编，对齐本仓库 ESP8266 + PlatformIO + Arduino 与 `src/` / `lib/` 布局。 |
| 2026-05-12 | 1.3：明确本仓库应用层采用 `setup`/`loop` 单线程模型，不引入 FreeRTOS 用户任务或 Ticker 伪任务拆分；与 `src/main.cpp` 约定一致。 |
