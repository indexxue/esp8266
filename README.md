# 文档目录

本目录用于存放与 ESP8266 工程相关的 Markdown 说明。**开发分层、编码约定、目录与工程规范**见 [`esp8266_embedded_standard.md`](esp8266_embedded_standard.md)。下文说明在本工程根目录下，如何用 **PlatformIO** 完成编译、烧录与串口监视。

---

## 环境说明

- 工程使用 **PlatformIO**，板型在根目录 `platformio.ini` 中配置（当前为 `nodemcuv2`）。若你的硬件不是 NodeMCU v2，请把 `board = ...` 改成对应 ID，可用命令列出：`py -3.11 -m platformio boards espressif8266`。
- 下文命令在 **PowerShell** 中执行；若已将 PlatformIO 加入 PATH，也可把 `py -3.11 -m platformio` 换成 `pio`。

## 进入工程目录

```powershell
Set-Location "d:\desktopsrc\TY\ESP8266"
```

（请按你本机实际路径修改。）

## 编译

仅编译固件、检查语法与依赖，不烧录：

```powershell
py -3.11 -m platformio run
```

成功时末尾会提示 `SUCCESS`，并在 `.pio/build/<环境名>/` 下生成固件。

**在 Cursor / VS Code 中**：安装 **PlatformIO IDE** 扩展后，可用底部状态栏或左侧 PlatformIO 面板的 **Build**（对勾图标）执行相同操作。

## 烧录（上传）

1. 用 USB 数据线连接开发板与电脑（需支持数据传输的线，不是仅充电线）。
2. 安装好 USB 转串口驱动（常见芯片 CH340 / CP2102 等，按你板子上的芯片选择官方驱动）。
3. 在工程根目录执行：

```powershell
py -3.11 -m platformio run -t upload
```

PlatformIO 会自动选择串口；若多块板或多串口，可指定端口（在设备管理器中查看 **端口 (COM 和 LPT)** 的 COM 号）：

```powershell
py -3.11 -m platformio run -t upload --upload-port COM5
```

也可在 `platformio.ini` 的 `[env:nodemcuv2]` 段增加固定端口，例如：

```ini
upload_port = COM5
```

部分最小系统板需在上传前按住 **FLASH**（或 **BOOT**）再点上传，松手时机以板子说明为准；NodeMCU 一类带自动下载电路的板子通常无需按键。

若上传失败，可尝试在 `platformio.ini` 同一环境中降低速率，例如：

```ini
upload_speed = 115200
```

## 查看串口设备（检测端口）

列出当前识别的串口，便于确认 COM 号：

```powershell
py -3.11 -m platformio device list
```

## 串口监视（查看 `Serial.print` 输出）

本工程 `src/main.cpp` 中使用 `Serial.begin(115200)`，监视器波特率需一致：

```powershell
py -3.11 -m platformio device monitor --baud 115200
```

指定端口示例：

```powershell
py -3.11 -m platformio device monitor --port COM5 --baud 115200
```

在 `platformio.ini` 中也可统一监视波特率，例如：

```ini
monitor_speed = 115200
```

配置后可直接：

```powershell
py -3.11 -m platformio device monitor
```

**在 Cursor / VS Code 中**：PlatformIO 面板中的 **Serial Monitor**（插头图标）打开监视器；若乱码，检查波特率是否与代码一致。

退出监视器：在终端中按 **Ctrl+C**。

## 一键编译并烧录

```powershell
py -3.11 -m platformio run -t upload
```

（与上文「烧录」相同：`run` 会先编译再执行 `upload` 目标。）

## 常见问题简述

| 现象 | 可尝试操作 |
|------|------------|
| 找不到串口 | 换 USB 口、换数据线、重装对应 USB 转串口驱动、在设备管理器中确认未标黄色感叹号 |
| 上传超时 / 失败 | 指定正确 `COM`、降低 `upload_speed`、检查板型是否与 `platformio.ini` 一致、按板子要求进入下载模式 |
| 串口监视无输出 | 确认波特率 115200、换 `COM`、上传后按一下复位键 |
| 编译提示缺平台/框架 | 保持网络畅通，首次会自动下载；也可执行 `py -3.11 -m platformio pkg install` 后重试 |

---

仍可按主题在本目录下拆分更多文件（例如 `flash.md` 专写各型号板子的烧录细节）。
