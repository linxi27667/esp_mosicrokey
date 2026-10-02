# 板级约束

更新：2026-10-02。固定开发基线为 ESP32-S31-Function-CoreBoard-1、ESP-IDF 6.1.0、target `esp32s31`。USB 实板已核对 S31 rev0.0、16 MiB Flash/PSRAM；用户已接 TFT 并报告黑屏，实际型号/供电待确认，显示验收未通过。按键、编码器和 TF 待确认接线。新八键/编码器及 J2 编号见[接线与操作指南](docs/tft-input-guide.md)。

## 固件默认配置

固定板载与扩展引脚集中在 `components/board_support/include/mosicrokey/board_config.hpp`；TFT 引脚、偏移及反色在 `main/Kconfig.projbuild` 的 MosiCrokey hardware 菜单。

| 功能 | GPIO / 配置 |
|---|---|
| TFT SCLK / MOSI / CS | 43 / 44 / 45 |
| TFT DC / RESET | 46 / 47 |
| TFT 背光控制 | 默认 -1：模块自行供电；有控制端时可设为 48 |
| TF D0 / D1 / D2 / D3 | 20 / 21 / 22 / 23 |
| TF CLK / CMD | 24 / 25 |
| 上 / 下 / 左 / 右 | 38 / 39 / 40 / 42 |
| 确认 / 返回 | 49 / 35 |
| 功能 / 模式 | 2 / 1 |
| 编码器 A / B / SW | 0 / 3 / 4 |
| ES8311 I2C SCL / SDA | 50 / 51 |
| I2S MCLK / BCLK / DIN / WS / DOUT | 52 / 53 / 54 / 55 / 56 |
| 功放使能 | 57 |
| UART TX / RX | 58 / 59 |
| RGB / BOOT | 60 / 61，保留板载用途 |
| 显示 | ST7735R 初始化，诊断默认 SPI2 10 MHz（Kconfig 可设到 20 MHz），160×128，MADCTL 0x68，BGR，偏移默认 0/0，默认不反色；不代表实际模块型号已识别 |
| TF | SDMMC slot 0，四线，20 MHz，FAT32，挂载 /sdcard |
| 按键 | 接地按下、内部上拉、低电平有效，5 ms 扫描 / 20 ms 去抖 |
| 编码器 | A/B 双边沿 GPIO ISR、四个合法边沿计一格、SW 共用 20 ms 去抖 |
| 音频 | ES8311，24 kHz，沿用上游音频处理链 |
| UART | 115200、8N1，文本命令 LF 结束 |

GPIO36/37 避开启动配置用途。GPIO60/61 同样保留板载及启动用途。扩展引脚按 S31 资料核对，不能套用 S3 引脚限制。

工程 Flash 配置为 16 MiB，应用分区 6 MiB；PSRAM 为 Octal、200 MHz，CPU 320 MHz。实板探测值已记录于历次 USB 验证，新的外设结论仍须附实际接线与同一固件日志。本机 SDK 将 S31 作为预览目标；通过构建不能替代实板兼容验证。

模块有不同存储容量的订购变体，容量表见[WROOM-3 模组数据手册](https://documentation.espressif.com/esp32-s31-wroom-3_datasheet_en.pdf)。当前镜像面向至少 16 MiB Flash 的板卡配置，不能直接用于 8 MiB Flash 变体。

## 接线前与首次上电核对

- TFT 模块电压、供电、初始化变体、BGR/RGB、列行偏移和背光电路；背光 GPIO 接控制端或驱动电路。颜色顺序、180° 翻转、偏移、反色和背光极性在 MosiCrokey Kconfig 中调整，重建后记录实际配置与哈希。
- TF 模块须是 3.3 V 四线 SDMMC 接口，具备所需上拉；仅引出 SPI 接口的模块不适用于当前四线默认接线。
- 外接按键与设备共地，避免占用板载音频、UART 和启动引脚。扬声器规格、功放连接、供电峰值及音量需实测。
- 首次确认 COM 口、芯片、Flash 和 PSRAM 后才烧录。保留启动、音频、屏幕与 TF 日志及固件哈希。

缺卡或挂载失败不会格式化。启动尝试挂载，用户可手动重挂；Agent 工作或游戏运行时拒绝重挂。拔卡/读写错误提示存储不可用，重新插卡后到设置页重挂。首版没有自动热插拔恢复。

## 资料依据

开发板资源依据[官方开发板指南](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-function-coreboard-1/user_guide.html)和[官方原理图](https://dl.espressif.com/schematics/esp32-s31-function-coreboard-1-schematics.pdf)，启动与引脚约束依据[S31 数据手册](https://documentation.espressif.com/esp32-s31_datasheet_en.html)。

完整键盘、电池、电源保护、外壳与无线外设属于后续硬件设计，当前工程未声明这些资源已验证。



