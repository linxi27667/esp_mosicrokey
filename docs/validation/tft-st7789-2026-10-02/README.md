# v0.2.3 TFT ST7789 诊断 — 第二轮

2026-10-02，用户最新提供规格为“1.8 寸、128×160、ST7789、SPI”。此前 v0.2.2 使用 ST7735 初始化，用户确认完全未亮，见[第一轮失败记录](../tft-black-2026-10-02/README.md)。本轮按新提供的规格适配；不将无法读取的商品页面或成功发送 SPI 数据作为实物芯片身份验证。

## 改动与配置

- Kconfig 默认选择 ST7789，保留旧 ST7735R 选项；两套初始化在编译期分开。
- ST7789 使用 RGB565 `COLMOD=0x55` 和大端 `RAMCTRL={0x00,0xf0}`，依据本机固定 ESP-IDF v6.1 的 `components/esp_lcd/src/esp_lcd_panel_st7789.c`。不向 ST7789 发送 ST7735 的电源、伽马表。
- 复位和退出休眠保留等待；格式、方向、反色设置完成后才进入正常显示并开屏。字节交换与 DMA 所有权规则保持一致。
- 160×128 横屏、SPI2 10 MHz、BGR、MADCTL 0x68、偏移 0/0、默认不反色。CLK43、MOSI44、CS45、DC46、RESET47；背光控制 -1，不操作 GPIO48，沿用用户无独立控制接线的说明。
- 诊断页面改为通用 SPI TFT 标识，避免在支持两种控制器时仍写死 ST7735。

实际供电与背光电路仍未确认。0/0 偏移是首轮配置，颜色、方向和可见区域须根据实体观察调整。

## 验证

源码提交：`0353ca494056b5ae48520e934e64bdcb4e065e8e`。64 个源码/配置/测试文件与该提交逐字节比对，哈希保存在 [record.json](record.json)。

固定 2960 个上游源码文件与六个补丁核验通过。ESP-IDF v6.1 / esp32s31 构建通过；应用 3,935,776 字节，6 MiB 应用分区余 37%。ELF 中 `app_main`、`create_board()`、`Application::Run()`、`claw_core_submit` 各有一个定义。

十个 CTest 均通过，包含 1457 项 C/C++ 检查；两种显示控制器各 40 项，验证格式/字节序/开屏顺序、旧控制器参数隔离、资源清理、边界与 DMA 生命周期。三个串口脚本故障用例和 11 项电脑连接回归通过。共享 LVGL 渲染 21 个页面状态，五种 RGB565 色值和完整边界检查通过；人工查看本轮诊断页与五色页截图，文字及布局正常。主机渲染不代表实体屏幕通过。

应用 SHA256：`4d9753032156c2bc48ffc9a0b92f187772afa03e3896ba7f212553abe1e10f20`。
ELF SHA256：`69b0ec98db6cc7836238997274851efd526c002830bc5792372dc33b25c712a1`。

COM8 CP210x 序列号 `7E6E5CC2F471F011A32BFB9E1045C30F` 与既有 ESP32-S31 rev0.0 设备一致。`idf.py -p COM8 app-flash` 只写应用，Flash 校验通过，保留 NVS 及资源。复位后启动日志确认：

```text
App version: 0.2.3
ELF file SHA256: 69b0ec98d...
ST7789 160x128 SPI=10000000Hz CLK=43 MOSI=44 CS=45 DC=46 RST=47 BL=-1 offset=0,0
SPI initialization complete; panel response is not probed
First LVGL pixels sent to SPI
```

执行 `page display` 后 `mode=9 wifi=1`，显示五色/四边框测试页的业务状态；捕获中未出现显示提交错误、崩溃或看门狗复位。设备没有面板回读接口，串口不能验证实际画面。

## 实体结果与剩余范围

**构建、主机回归、烧录和软件 SPI 提交通过；用户随后确认仍未点亮，本轮实体验收失败。** 实物照片显示七个端子按左到右为 RST、CS、DC、SDA、SCL、GND、VCC，确实没有独立背光控制端。用户确认 VCC 接板上标记 3V3 的供电脚，尚无屏幕端电压测量。照片未覆盖开发板接线端或屏幕后部电路，不能验证整条线的导通或背光供电。

后续 COM8 状态仍为 v0.2.3、`mode=9 wifi=1`，采集中没有崩溃/看门狗日志；再次停留五色页，见 [未亮复查日志](evidence/still-dark.txt)。0/0 偏移、颜色等均未通过实体确认。

若有画面但颜色或裁切异常，按观察调整 BGR、反色、方向和偏移；若仍完全不亮，需核对实际 VCC/GND、供电电压和背光端电路。此前失败记录保留，本轮不宣称完整语音、外部 TF/按键/编码器或两小时稳定性通过。

本轮属于 `codex/tft-bringup` 诊断分支。证据见 [record.json](record.json)、[启动日志](evidence/boot.txt)、[五色页状态](evidence/display-test.txt)、[显示配置](evidence/lcd-config.h) 和 [主机五色页](evidence/display-test.png)。
