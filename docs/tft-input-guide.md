# 屏幕、八键与编码器接入（v0.2.3 TFT 适配）

用户最新提供屏幕规格为 1.8 寸、128×160、ST7789、SPI 接口。v0.2.3 默认按此控制器适配为 160×128 横屏，保留八个低有效按键与带按压的 EC11 编码器方案；旧 ST7735R 可在 Kconfig 选择。实际芯片没有回读确认，屏幕可见结果仍须实测。

## 界面参考与落地

2026-10-02 查阅以下原作者资料，只参考交互和布局，没有复制代码、图片或图标：

| 参考 | 本项目采用的想法 |
|---|---|
| [ESP32 Console](https://github.com/emir173/esp32-console) | 同样的 160×128 TFT 掌机菜单；使用明确焦点和独立游戏画布 |
| [TheDeck](https://github.com/ItsAkshatSh/thedeck) | 根据当前用途切换媒体、应用和设置页；编码器参与媒体控制 |
| [Elgato Stream Deck Neo](https://www.elgato.com/us/en/p/stream-deck-neo) | 八键控制器的功能分组；在屏幕上显示当前按键含义 |

这些是参考方案，不是可直接烧录到 S31 的设计稿。当前实际 LVGL 页面采用深色、六入口双列首页、青色设备焦点、橙色 Agent 和蓝色电脑页。页面源码与主机渲染共用，见[本轮截图](validation/tft-input-2026-10-02/evidence/ui/contact-sheet.png)。

## 接线

GPIO 与 J2 物理编号已对照[官方 J2 表](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-function-coreboard-1/user_guide.html#header-block)、[原理图](https://dl.espressif.com/schematics/esp32-s31-function-coreboard-1-schematics.pdf)及 [S31 数据手册](https://documentation.espressif.com/esp32-s31_datasheet_en.pdf)。GPIO0/1 在 S31 上不是本项目避开的启动配置脚；不要套用其他 ESP32 型号的引脚规则。按 PCB 丝印核对排针方向，再连接。

| 外设端 | GPIO | J2 编号 |
|---|---:|---:|
| TFT SCLK / SCK | 43 | 17 |
| TFT MOSI / SDA / DIN | 44 | 18 |
| TFT CS | 45 | 15 |
| TFT DC / A0 | 46 | 16 |
| TFT RESET / RES | 47 | 13 |
| TFT 背光控制（可选） | 48 | 14 |
| K1 上 | 38 | 22 |
| K2 下 | 39 | 21 |
| K3 左 | 40 | 19 |
| K4 右 | 42 | 20 |
| K5 确认 | 49 | 11 |
| K6 返回 | 35 | 25 |
| K7 功能 | 2 | 7 |
| K8 模式 | 1 | 12 |
| 编码器 A / CLK | 0 | 9 |
| 编码器 B / DT | 3 | 10 |
| 编码器 SW | 4 | 32 |
| 3.3 V | — | 35 或 36 |
| GND | — | 1、2、8、33、34、37 或 38 |

按键一端接 GPIO、一端接 GND；固件开启上拉，稳定 20 ms 后记录按下/释放。编码器公共端接 GND，A/B 接上述 GPIO，按压触点另一端接 GND；有供电的模块使用 3.3 V。A/B 接反会颠倒旋转方向，可以对调接线。默认四个有效相位边沿计一格；不同编码器触点形式要据实调整 `board_config::encoder_edges_per_detent`。合法反向跳变抵消弹跳，非法双位跳变丢弃未完成格，GPIO ISR 采集避免 5 ms 扫描漏掉快速边沿。

屏幕供电和背光须按实际模块规格接。GPIO48 接背光逻辑控制端或驱动电路；如果模块只有 LED 电源端，按规格接供电并将 `MOSI_LCD_BL=-1`，不要由 GPIO 直接承担未知背光电流。所有外设共地，接线时断开供电。

用户表示没有接入独立背光控制，诊断版本默认 `MOSI_LCD_BL=-1`，不配置 GPIO48；SPI 初始为 10 MHz，通过 `MOSI_LCD_SPI_HZ` 调整。实际供电与背光要求仍以模块电路为准。此前商品截图与尺寸描述不一致；此次按用户新提供的 ST7789/128×160 规格切换驱动，不将商品页面视作芯片身份验证。

`MosiCrokey hardware` → `TFT controller` 选择 ST7789 或 ST7735R。ST7789 使用 `COLMOD=0x55` 与大端 RAMCTRL；ST7735R 保留其 `0x05` 格式和独立初始化表。两种驱动均在方向、颜色及反色配置完成后开启显示。不要混用两者的初始化参数。

此分配保留 TF 的 GPIO20–25、音频 GPIO50–57、串口 GPIO58–59、RGB GPIO60、BOOT GPIO61，避开 GPIO36/37。参数集中在 `board_config.hpp` 和 `main/Kconfig.projbuild`。

## 默认操作

| 输入 | 设备模式 | 电脑媒体组 | 电脑快捷键组 |
|---|---|---|---|
| K1 上 | 首页上一行；列表/文本向上 | 音量增加 | Ctrl+C 复制 |
| K2 下 | 首页下一行；列表/文本向下 | 音量降低 | Ctrl+V 粘贴 |
| K3 左 | 上一入口/条目 | 上一首 | Ctrl+Z 撤销 |
| K4 右 | 下一入口/条目 | 下一首 | Ctrl+Y 重做 |
| K5 确认 | 进入、执行；小智开始/中断；忙碌 Agent 取消 | 播放/暂停 | Ctrl+S 保存 |
| K6 返回 | 上一级；游戏退出至游戏库 | 静音 | Tab |
| K7 功能 | 回首页 | 切换为快捷键组 | 切换为媒体组 |
| K8 模式 | 进入电脑页，释放语音/游戏焦点 | 回到先前设备页 | 回到先前设备页 |
| 旋钮转动 | 按格选择或滚动；游戏中不改变方向 | 音量 | 音量 |
| 旋钮按压 | 相当于确认；游戏中保留按下/释放时长 | 静音 | 静音 |

首页上下按双列跨度移动，左右和旋钮按单项移动，边界停留不循环。游戏 K1–K4 是方向，K5/旋钮按压是确认，K6 退出、K7 首页、K8 电脑页。离开游戏释放 VM；从电脑模式返回游戏库，不恢复已经停止的 VM。Agent 请求可继续在后台执行。

检测页例外：K1–K5、K7、K8 和旋钮按压只更新指示，不触发导航；K6 返回设置。因此 K6 的按下/释放可以通过 `input_status` 观察，其余按键在屏幕上逐个亮起。计数是输入边沿数/累计旋转格数，不是电脑实际音量百分比。模式与电脑映射暂为固定两组，不保存自定义宏；后续确定用途后再扩展。

## 接线后验证顺序

1. 启动应看到六入口首页。设置 → 屏幕颜色与边界：五条应依次为红、绿、蓝、白、黑，四边白框与 TL/TR/BL/BR 完整。
2. 红蓝颠倒调整 `MOSI_LCD_BGR`；倒置调整 `MOSI_LCD_ROTATE_180`；边缘裁切按模块调整 X/Y offset；负片调整 `MOSI_LCD_INVERT`；背光电平相反调整 `MOSI_LCD_BL_ACTIVE_LOW`。通过 menuconfig 改动后重新编译、烧录并记录哈希。
3. 设置 → 按键与旋钮检测：逐个按住/释放，1–8/E 指示对应；旋转一格只计一次、方向一致。先验证慢转，再验证正常速度，不凭串口注入代替触点实测。
4. 设置 → 试玩贪吃蛇：生成受限 Lua RAM 预览，在游戏库确认启动，验证四方向、得分、失败后确认重新开始、返回退出。该预览重启丢失，TF 状态仍为不可用；持久化用 `demo` 且须接入 TF。
5. 在电脑启动连接程序，再按 K8 切换媒体组。先以 `--dry-run` 检查动作/ACK，随后退出并正常启动，验证旋钮音量、按压静音、媒体键和两组快捷键。

## 电脑连接程序

使用现有 **USB-to-UART** 接口即可；它仍枚举为 CP210x 串口。当前版本实现串口配套程序，没有将该接口枚举为 USB HID 键盘。直接 HID 需要后续确认 S31 USB 设备接口和接线。

```powershell
# 本机已存在带 pyserial 的 ESP-IDF Python 环境
& 'E:\MCU\esp32\.espressif\v6.1\tools\python_env\v6.1\venv\Scripts\python.exe' scripts/pc_bridge.py --port COM实际编号 --dry-run
# 正常执行 Windows 电脑媒体/快捷键操作
& 'E:\MCU\esp32\.espressif\v6.1\tools\python_env\v6.1\venv\Scripts\python.exe' scripts/pc_bridge.py --port COM实际编号
```

连接程序每秒发送会话心跳，固件 3 秒无心跳则停发；待确认动作 1.5 秒超时且不重发。电脑动作只来自固定白名单，输入队列延迟超过 500 ms 或会话变更即丢弃。离线按键不在重连后补发；电脑端按递增序号去重。每次键操作包含按下/抬起，部分 SendInput 失败会释放修饰键并回报失败。

电脑休眠恢复或检测到设备重新启动时更换会话，丢弃旧缓冲；读取期间发生暂停也不执行旧动作。`--dry-run` 回传模拟确认，设备显示“电脑模拟确认”，真实成功才显示“电脑已执行”。

电脑程序使用 Windows [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)，受目标窗口权限等级影响；失败显示在设备上。程序退出后约 3 秒显示断开。串口由一个程序独占，烧录或串口终端使用前退出连接程序。默认不安装自启动项，也不读取电脑内容。

## 串口辅助

```text
page inputs
input_status
key up 1
key up 0
key function 1
key function 0
encoder 1
encoder -1
page display
page settings
preview_demo
page games
key confirm 1
key confirm 0
key back 1
key back 0
page pc
```

`pc_link`、`pc_ack` 是连接程序协议命令，普通用户无需手动输入。所有串口输入仍不回显、不记录命令历史。实际屏幕、触点、编码器与电脑键控结论须由接线后的日志/观察补齐；主机截图和测试不替代实物验收。

## 黑屏诊断

先区分完全不发光与有背光无画面。按实际模块标识确认 VCC、GND、SCL/SCK、SDA/MOSI、RES、DC、CS；这里 SCL/SDA 是 SPI 信号。GPIO43 等指的是板上 GPIO 丝印，不是 J2 的物理编号。不要在型号不明时直接将供电改到 5 V。

构建脚本先执行 `idf.py reconfigure` 再构建，避免修改项目 Kconfig 后仍使用旧生成配置。启动日志 `mosi_lcd` 打印实际引脚、SPI 频率、背光配置与偏移；`First LVGL pixels sent to SPI` 表示驱动完成了一次提交。当前没有接回读信号，日志不能证明面板已响应，也不能自动识别驱动芯片。`page display` 保持五色/边框页，实体是否显示须观察记录。
