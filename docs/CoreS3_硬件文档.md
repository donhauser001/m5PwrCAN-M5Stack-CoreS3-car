# M5Stack CoreS3 硬件文档

> 产品型号：K128 | 官方文档：https://docs.m5stack.com/zh_CN/core/CoreS3

---

## 1. 产品概述

CoreS3 是 M5Stack 开发套件系列的第三代主机，核心主控采用 **ESP32-S3** 方案（双核 Xtensa LX7，主频 240MHz），内置 Wi-Fi，板载 16MB Flash 和 8MB PSRAM。正面搭载 2.0 寸电容触摸 IPS 屏，屏幕下方集成 0.3MP 摄像头（GC0308）和接近传感器（LTR-553ALS-WA）。采用 AXP2101 低功耗电源管理芯片，内置 500mAh 锂电池。

套装默认附带 DinBase 底座，支持 Din 导轨、挂墙及螺丝固定安装方式，可外部 DC 12V（9~24V）供电。

---

## 2. 核心规格参数

| 规格             | 参数                                              |
| ---------------- | ------------------------------------------------- |
| SoC              | ESP32-S3，双核 Xtensa LX7，主频 240MHz            |
| Flash            | 16MB                                              |
| PSRAM            | 8MB                                               |
| Wi-Fi            | 2.4 GHz                                           |
| USB              | Type-C（支持 OTG 和 CDC）                         |
| 触摸 IPS LCD 屏幕 | 2.0 寸，320×240，ILI9342C，高强度玻璃面板         |
| 摄像头           | GC0308，0.3MP                                     |
| 接近传感器       | LTR-553ALS-WA                                     |
| 电源管理芯片     | AXP2101                                           |
| IO 扩展芯片      | AW9523B                                           |
| 六轴姿态传感器   | BMI270（3 轴陀螺仪 + 3 轴加速计）                 |
| 三轴磁力计       | BMM150（挂载于 BMI270 辅助 I2C）                   |
| RTC              | BM8563                                            |
| 扬声器           | 1W，AW88298 16bits-I2S 功放                       |
| 音频编码芯片     | ES7210，双麦克风输入                               |
| 电池             | 500mAh 锂电池                                     |
| 主机尺寸         | 54.0 × 54.0 × 15.5mm                              |
| 套件尺寸（含底座）| 69.0 × 54.0 × 31.5mm                              |
| 重量             | 72.7g                                             |

---

## 3. I2C 设备地址表

所有内部 I2C 设备共享系统 I2C 总线（SDA: G12, SCL: G11）。

| 芯片             | I2C 地址 | 功能           |
| ---------------- | -------- | -------------- |
| AXP2101          | 0x34     | 电源管理       |
| AW88298          | 0x36     | I2S 功放       |
| FT6336U          | 0x38     | 电容触摸控制   |
| ES7210           | 0x40     | 音频编码       |
| BM8563           | 0x51     | RTC 时钟       |
| AW9523B          | 0x58     | IO 扩展        |
| BMI270           | 0x69     | 六轴 IMU       |
| BMM150           | 0x10     | 磁力计 *       |
| GC0308           | 0x21     | 摄像头         |
| LTR-553ALS-WA   | 0x23     | 接近传感器     |

> \* BMM150 挂载于 BMI270 的 Sensor Hub 辅助 I2C 接口，非直接连接到系统 I2C 总线。

---

## 4. 引脚映射

### 4.1 LCD 屏幕（SPI）

| ESP32-S3 引脚 | 功能   |
| -------------- | ------ |
| G37            | MOSI   |
| G36            | SCK    |
| G3             | CS     |
| G35            | DC     |

- 复位（RST）：由 AW9523B P1_1 控制
- 背光（BL）：由 AXP2101 DLDO1 控制
- 电源（PWR）：由 AXP2101 LX1 控制

### 4.2 microSD 卡（SPI，最大支持 16GB）

| ESP32-S3 引脚 | 功能   |
| -------------- | ------ |
| G35            | MISO   |
| G37            | MOSI   |
| G36            | SCK    |
| G4             | CS     |

### 4.3 摄像头 GC0308

| 功能                 | ESP32-S3 引脚 |
| -------------------- | -------------- |
| SCCB Clock (SIOC)    | G11            |
| SCCB Data (SIOD)     | G12            |
| VSYNC                | G46            |
| HREF                 | G38            |
| PCLK                 | G45            |
| D0                   | G39            |
| D1                   | G40            |
| D2                   | G41            |
| D3                   | G42            |
| D4                   | G15            |
| D5                   | G16            |
| D6                   | G48            |
| D7                   | G47            |
| RESET                | AW9523B P1_0   |

### 4.4 电容触摸屏 FT6336U

| 功能       | 引脚           |
| ---------- | -------------- |
| SDA        | G12            |
| SCL        | G11            |
| TOUCH_RST  | AW9523B P0_0   |
| TOUCH_INT  | AW9523B P1_2   |

### 4.5 麦克风与功放（I2S）

| ESP32-S3 引脚 | 功能       |
| -------------- | ---------- |
| G34            | I2S_BCK    |
| G33            | I2S_WCK    |
| G13            | I2S_DATO   |
| G14            | I2S_MCLK   |
| G0             | I2S_DATI   |

- 功放复位（AW_RST）：AW9523B P0_2
- 功放中断（AW_INT）：AW9523B P1_3

### 4.6 IMU（BMI270 + BMM150）

| 功能 | 引脚 |
| ---- | ---- |
| SDA  | G12  |
| SCL  | G11  |

> BMM150 通过 BMI270 的辅助 I2C（BMI270_ASDx / BMI270_ASCx）接入。

### 4.7 RTC（BM8563）

| 功能 | 引脚 |
| ---- | ---- |
| SDA  | G12  |
| SCL  | G11  |

- 唤醒信号连接至 AXP2101 的 AXP_WAKEUP 引脚。

---

## 5. 外部接口（HY2.0-4P）

| 端口     | GND   | VCC  | 引脚 1 | 引脚 2 | 典型用途       |
| -------- | ----- | ---- | ------ | ------ | -------------- |
| PORT.A   | GND   | 5V   | G2     | G1     | I2C / GPIO     |
| PORT.B   | GND   | 5V   | G9     | G8     | GPIO / ADC     |
| PORT.C   | GND   | 5V   | G17    | G18    | UART / GPIO    |

---

## 6. M5-Bus 总线引脚定义

| 功能           | 引脚 | 左侧 | 右侧 | 引脚     | 功能       |
| -------------- | ---- | ---- | ---- | -------- | ---------- |
| GND            | 1    |      |      | G10      | ADC        |
| GND            | 3    |      |      | G8       | PB_IN      |
| GND            | 5    |      |      | RST      | EN         |
| MOSI           | G37  | 7    | 8    | G5       | GPIO       |
| MISO           | G35  | 9    | 10   | G9       | PB_OUT     |
| SCK            | G36  | 11   | 12   | 3V3      |            |
| RXD0           | G44  | 13   | 14   | G43      | TXD0       |
| PC_RX          | G18  | 15   | 16   | G17      | PC_TX      |
| Int SDA        | G12  | 17   | 18   | G11      | Int SCL    |
| PORT.A SDA     | G2   | 19   | 20   | G1       | PORT.A SCL |
| GPIO           | G6   | 21   | 22   | G7       | GPIO       |
| I2S_DOUT       | G13  | 23   | 24   | G0       | I2S_LRCK   |
| HVIN(Base DIN) | 25   |      |      | G14      | I2S_DIN    |
| HVIN(Base DIN) | 27   |      |      | 5V       |            |
| HVIN(Base DIN) | 29   |      |      | BAT      |            |

---

## 7. 电源系统

### 7.1 电源架构

- **电源管理芯片**：AXP2101（I2C 地址 0x34）
- **IO 扩展芯片**：AW9523B（I2C 地址 0x58），用于控制电源输入输出方向
- **电池**：内置 500mAh 锂电池
- **外部供电**：DinBase 底座支持 DC 9~24V 输入

### 7.2 电源控制引脚

通过 AW9523B 的 `BUS_OUT_EN` 和 `USB_OTG_EN` 引脚控制电源输入输出方向。

### 7.3 电源指示灯

| 指示灯   | 连接         | 说明               |
| -------- | ------------ | ------------------ |
| Red LED  | AXP2101 RTC_VDD | 充电状态指示灯   |

---

## 8. 操作说明

| 操作             | 方法                                   |
| ---------------- | -------------------------------------- |
| 开机             | 单击左侧电源键                         |
| 关机             | 长按左侧电源键 6 秒                    |
| 复位             | 单击底侧 RST 按键                     |
| 进入下载模式     | 长按复位键 3 秒（绿灯亮起）           |

---

## 9. 开发环境配置

### 9.1 支持的开发平台

- Arduino IDE（配合 M5Unified / M5GFX 库）
- UiFlow2（图形化编程）
- ESP-IDF
- PlatformIO

### 9.2 PlatformIO 配置参考

```ini
[env:m5stack-cores3]
platform = espressif32@6.7.0
board = esp32-s3-devkitc-1
framework = arduino
upload_speed = 1500000
monitor_speed = 115200
build_flags =
    -DESP32S3
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DCORE_DEBUG_LEVEL=5
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    M5Unified=https://github.com/m5stack/M5Unified
```

### 9.3 Arduino 依赖库

| 库名称      | 用途                   | 来源                                        |
| ----------- | ---------------------- | ------------------------------------------- |
| M5Unified   | 统一硬件抽象层         | https://github.com/m5stack/M5Unified        |
| M5GFX       | 图形显示库             | https://github.com/m5stack/M5GFX            |

---

## 10. 注意事项

1. **磁场干扰**：带有磁铁的产品可能对 BMM150 磁力计造成干扰，导致读数异常。搭配含磁铁的 M5 设备时需拆除磁铁，避免传感器放置在强磁场附近。
2. **下载模式**：烧录程序前需长按复位键 3 秒进入下载模式（绿灯亮起）。
3. **microSD 卡**：最大支持 16GB。
4. **LCD 与 SD 卡共享 SPI 总线**：LCD 和 microSD 共用 MOSI (G37)、SCK (G36) 引脚，通过不同的 CS 引脚区分（LCD: G3, SD: G4）。

---

## 11. 相关资料链接

| 资料             | 链接                                                              |
| ---------------- | ----------------------------------------------------------------- |
| 官方产品文档     | https://docs.m5stack.com/zh_CN/core/CoreS3                       |
| 原理图 (CoreS3)  | 官方文档页面内下载                                                |
| 原理图 (DinBase) | 官方文档页面内下载                                                |
| ESP32-S3 数据手册 | 乐鑫官网                                                         |
| Arduino 快速上手  | https://docs.m5stack.com/zh_CN/arduino/cores3/arduino_tutorial    |
| UiFlow2 快速上手  | https://docs.m5stack.com/zh_CN/uiflow2/cores3/uiflow2_tutorial   |

---

*文档版本：v1.0 | 创建日期：2026-02-13*
