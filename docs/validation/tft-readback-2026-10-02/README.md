# v0.2.4 TFT SDA 回读诊断 — 第三轮

用户实物照片确认屏幕排针从左到右为 RST、CS、DC、SDA、SCL、GND、VCC，七针且无背光控制端；用户确认 VCC 接板上标记 3V3 的供电脚，屏幕端实际电压未测。v0.2.3 用户确认仍未亮，已将[第二轮记录](../tft-st7789-2026-10-02/README.md)更新为实体验收失败。

## 本轮改动

默认仍为用户提供的 ST7789/128×160，横屏 160×128，未改变接线或未知供电。仅在启动和首帧之前使用原 MOSI/SDA 尝试回读，UI/游戏期间使用原单向像素写路径。

1. 用 1 MHz SPI 和 SDK `sio_mode` 创建临时 IO，保留独立 DC、8 bit 命令与参数；CS 预留一个周期建立时间。LCD 接口仍是四线协议。
2. 初始化控制器后读取 RDID1/2/3、MADCTL、COLMOD。单字节读取采用八个数据时钟，依据 [Sitronix ST7789V 数据手册](https://dl.espressif.com/dl/schematics/ST7789V_SPEC_V1.0.pdf) 第 8.4 节与固定 ESP-IDF v6.1 LCD SPI IO 实现。模块绑带为分离 SDO 或存在单向电路时，当前七针未必支持 SDA 回读。
3. 在首帧前短暂翻转 MADCTL 的 MY 位，并恢复原方向。只有原值、变化值、恢复值连续与预期吻合才确认寄存器往返；固定 00/FF 和 SDK 返回成功均不足以确认响应。
4. 方向恢复失败则返回初始化错误；恢复成功后删除临时 IO，重新创建原 10 MHz 单向像素 IO。回读不可用仍允许正常绘图。初始化、恢复、重建失败均释放已持有资源。

## 软件与烧录证据

源码提交：`7c09f6cd44313e323da6bb6d3d2482cc7d012144`，66 个源码/配置/测试文件与提交逐字节比对。上游 2960 文件、六补丁核验通过。固定 ESP-IDF v6.1 / esp32s31 构建通过，最终增量构建包含更新的头文件。

十个 CTest、1531 项 C/C++ 检查通过，两种控制器各 77 项，包含低速启动回读、模拟面板方向变化、回读失败后恢复原写 IO、方向恢复失败清理和绘制 IO 重建失败清理。三个串口脚本故障用例、11 项电脑连接回归与 21 个共享 LVGL 状态渲染通过。本轮页面源码未改变；未宣称实体显示或新镜像长测通过。

应用大小：3,936,752 字节，6 MiB 应用分区余 37%。
应用 SHA256：`3d38e149c07c6583117b12c8fdb6df4ea1aacf3402a5865bec7e3e3b45e1c778`。
ELF SHA256：`f0ab6f85362d48f7ac0f8e2f06db9050eda25760066b3727a81dd074b706b065`。
唯一 `app_main`、板工厂、真实小智 Run 与 claw_core_submit 定义已检查。

COM8 CP210x `7E6E5CC2F471F011A32BFB9E1045C30F`，ESP32-S31 rev0.0。正常 `app-flash` 只写应用并通过 Flash 校验，保留 NVS 与资源。复位后的实际日志：

```text
App version: 0.2.4
Probe SDA=44 clock=1000000Hz ID=00,00,00 errors=0,0,0
Probe MADCTL=00->00->00 expected=68->e8->68 errors=0,0,0,0 confirmed=0 COLMOD=00 error=0
No confirmed bidirectional panel response; check power, wiring and module read support
ST7789 160x128 SPI=10000000Hz CLK=43 MOSI=44 CS=45 DC=46 RST=47 BL=-1 offset=0,0
Pixel SPI initialized; visible display still requires physical observation
First LVGL pixels sent to SPI
```

`page display` 后 `mode=9 wifi=1`、v0.2.4，捕获中未出现崩溃/WDT/显示提交错误。完整日志、配置、源码和产物哈希见 [record.json](record.json) 与 [启动日志](evidence/boot.txt)。

## 结论与剩余检查

**本轮没有确认双向面板响应。** 所有读数为 00，未随可恢复方向写入变化；SDK 事务成功只说明主控执行了操作。结果不能单独证明屏幕坏了、没有供电或控制器型号错误，也可能是模块不支持现有 SDA 回读。

v0.2.4 已恢复正常像素 IO 并停在五色测试页，其实体可见结果仍待新观察。下一步需要在屏幕焊盘处测 VCC-GND 与 RST-GND，并查看模块背面电路和开发板端接线，确认导通、供电与背光。原照片未显示这些位置。引脚按[官方 J2 表](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-function-coreboard-1/user_guide.html#header-block)核对，七针逐项映射见[接线指南](../../tft-input-guide.md)。
