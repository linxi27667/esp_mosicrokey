# v0.2.2 TFT 黑屏诊断 — 第一轮

2026-10-02，用户已接入 TFT，报告完全没有点亮，并表示没有独立背光控制脚。用户描述为 1.8 寸，提供的商品截图却标注 2.4 寸、ST7789、240×320 且有 BLK。用户暂不能确认型号，授权先闭环试验；本轮按原 1.8 寸 ST7735/160×128 基线试验，不把商品图当作实际芯片确认。

## 观察与改动

COM8 CP210x `7E6E5CC2F471F011A32BFB9E1045C30F` 与既有设备一致。烧录前 UI 任务正常，`page display` 命令接受；旧生成 sdkconfig.h 中缺少新加入的 BGR/旋转/背光极性选项。软件能提交 SPI 数据，不代表面板已收到。

- 构建脚本先 `idf.py reconfigure`，再增量构建，明确刷新项目 Kconfig。
- 无独立背光控制默认 `MOSI_LCD_BL=-1`，不配置 GPIO48；已有本地 sdkconfig 同步改为 -1，NVS 不变。
- 将 SPI 频率集中为 `MOSI_LCD_SPI_HZ`，诊断默认 10 MHz，可调 1–20 MHz。
- 启动打印 SPI/引脚/背光/偏移；首次 LVGL DMA 完成后只打印一次提交日志，明确不做面板响应探测。
- 增加主机检查：模块自行供背光时不操作 GPIO48；原 DMA 资源/超时检查保留。

引脚仍为 CLK43、MOSI44、CS45、DC46、RESET47；这些是 GPIO 丝印，不是排针编号。供电及实物全部排针仍待确认。图中 BLK 是背光相关端，若实际也有而悬空，需要核对模块电路；不能据未确认商品图直接改供电到 5 V。

## 验证结果

v0.2.2 构建通过，应用 3,936,016 字节、6 MiB 分区余 37%；固定 2960 上游文件与六个补丁校验通过。九个 CTest、三个串口脚本故障用例和 11 个电脑连接回归通过；C++ 断言 1411，其中显示 HAL 34 项。

应用 SHA256：`ce9ce88ce3ea3ac79b9603a8b02a9fb73819aba7ad273f1c5201de52a572be2e`。
ELF SHA256：`1a8f802fd19a997b11beaedc0515722f90d6af7553aa69fcef6adc20ae326962`。

COM8 正常 `app-flash` 校验成功，只写应用并保留 NVS/既有资源。随后复位启动，日志确认：

```text
App version: 0.2.2
ST7735 160x128 SPI=10000000Hz CLK=43 MOSI=44 CS=45 DC=46 RST=47 BL=-1 offset=0,0
SPI initialization complete; panel response is not probed
First LVGL pixels sent to SPI
```

`page display` 后状态 `mode=9 wifi=1`，显示任务运行，未捕获显示传输错误或复位。用户随后反馈“完全没亮”，所以第一轮实体显示验收失败。后续状态仍为 v0.2.2、UI 任务正常、POWERON 启动原因，采集中未出现崩溃/看门狗；设备重新位于首页后再次切回五色/边框测试页。供电或背光实际接线尚未获得确认。

## 当前结论与下一步

**本轮软件构建、烧录、配置刷新和 SPI 提交通过；用户确认实体完全未点亮，黑屏问题未解决。** 当前没有面板回读连接，串口日志不能自动识别控制器或确认屏幕发光。先核对实物 VCC/GND 与 BLK/BL/LED 端，不根据无响应继续更换初始化参数或把未知供电改为 5 V。

若有画面，继续核对颜色、方向及边缘；若有背光无画面，再依据实际标识和观察试验控制器/初始化及 SPI 接线；若完全不发光，先核对 VCC/GND 和是否实际存在 BLK/BL/LED 端。每一轮保留独立镜像哈希，不将此短诊断复用为全功能或长测通过。

资料：[官方 J2 引脚表](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-function-coreboard-1/user_guide.html#header-block)；[Adafruit 原作者 ST7735 初始化源码](https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST7735.cpp)。ST7789 的协议对照使用本机 ESP-IDF v6.1 `components/esp_lcd/src/esp_lcd_panel_st7789.c`，本轮尚未切换驱动。

本轮日志、生成配置和哈希见 [record.json](record.json) 与 `evidence/`。诊断分支为 `codex/tft-bringup`；v0.2.1 的既有结果保持历史归属。
