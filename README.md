# MosiCrokey v0.2.4 TFT 诊断分支

用户提供屏幕规格为 1.8 寸 ST7789、128×160；当前分支默认 ST7789 的 160×128 横屏，保留 ST7735R 配置选项。实物照片确认七针、无背光端；VCC 用户确认接 3V3，实际电压未测。v0.2.2 和 v0.2.3 的实体结果均为完全未亮。正常绘制采用 GPIO 背光 -1、SPI 10 MHz；不能凭 SPI 提交成功认定屏幕通过。当前按键、编码器和 TF 的接线仍待确认。

v0.2.3 通过十个 CTest、1457 项检查并保留 NVS 烧录到 COM8，但用户确认仍未亮，见[ST7789 第二轮失败记录](docs/validation/tft-st7789-2026-10-02/README.md)。v0.2.4 新增启动阶段 1 MHz SDA 双向寄存器探测；改变并恢复方向值后检查读数是否随之变化，探测结束恢复原绘制 IO。无回读不阻止正常写路径；方向恢复或 IO 重建失败才返回初始化错误。

v0.2.1 增加八按键、带按压编码器、六入口首页、电脑媒体/快捷键、输入检测、屏幕校准和设置内的临时贪吃蛇，当时默认 ST7735。电脑通过 USB 串口配套程序操作，旋钮调音量、按压静音，K7 切组、K8 切换设备/电脑。接线表与完整默认映射见[屏幕与输入指南](docs/tft-input-guide.md)，历史证据见[v0.2.1 验证记录](docs/validation/tft-input-2026-10-02/README.md)。

自有代码采用直接 HAL → APP 调用，Task 负责调度；`Hal_Display_Init`、`Page_Navigate`、`Game_Update_Process` 命名保持一致。六个 HAL 管理显示、TF、按键、编码器、UART 与音频 I²C；真实上游音频/Agent 保留各自生命周期。详细调用链见[架构](docs/architecture.md)，规范见[架构 skill](skills/embedded-hal-app/SKILL.md)。

源码仓库：[linxi27667/esp_mosicrokey](https://github.com/linxi27667/esp_mosicrokey)，私有，默认分支 main。开发与验收变更通过 Git 提交记录，见[Git 管理流程](docs/git-workflow.md)。

当前目录是独立的 ESP-IDF 工程：真实小智组件、真实 ESPClaw Agent、TF 游戏版本库、受限 Lua 运行时和共享 LVGL 页面已实现。2026-10-01 已在 COM8 的 ESP32-S31 上烧录、联网并完成真实智谱请求和 Agent 贪吃蛇生成；当时仅连接 USB，游戏采用明确标注的临时内存预览。

v0.2.1 已通过 1410 项主机断言、九个 CTest、三个串口脚本故障用例、11 项电脑连接回归和 21 个共享页面渲染。COM8 已保留 NVS 烧录；33 项 USB 冒烟检查通过，覆盖九路输入、旋钮、校准页、15 个电脑动作的模拟确认、心跳过期和临时贪吃蛇启动/画布/退出。游戏期间真实 GLM 请求完成，回复与最终空闲状态已核对。实体屏、按键/旋钮、电脑实际键控和 TF 持久化待接线验收；小智仍等待账号激活，完整语音尚未实测。

历史 v0.2.0 的真实 GLM、Agent 改编发布及 32 项导出源码玩法检查见[前轮记录](docs/validation/hal-app-2026-10-02/README.md)。模型首轮空壳及查找失败均保留，不能只凭工具成功认定玩法完成。v0.1.9 的长测属于历史固件，详见[重连记录](docs/validation/hardware-2026-10-02/README.md)；此前串口中断的部分记录不合并时长。

历史 v0.1.9 独立 USB 长测连续 7201.29 秒完成：100 次游戏加载退出、600 条导航命令，稳定阶段最低/平均 30.3 FPS，无已记录崩溃/复位/WDT；24 次后台请求中21成功、3次断网连接失败，自动重连后连续4次成功。GitHub 已发布的 v0.1.9 标签和附件保持原版本；v0.2.1 不沿用它的两小时通过结论。

## 快速使用

在当前项目根目录的 PowerShell 执行：

```powershell
./scripts/build.ps1
./scripts/test-host.ps1
./scripts/package-release.ps1
```

- SDK：本机 ESP-IDF 6.1.0，target 为 `esp32s31`。
- 开发板：ESP32-S31-Function-CoreBoard-1；默认显示 ST7789、160×128 横屏。
- 版本：小智 `ed5cd7ba0011dc06fefd0762790857268c41b430`，ESPClaw `74b18700a1d6c40de472bfc71c19e49356ca1cc0`，LVGL 9.5.0，Lua 5.5.0~7。
- 当前源码 app：`build/mosicrokey.bin`，v0.2.4；应用分区偏移 0x20000。各轮实际烧录版本、大小与哈希以 `docs/validation/` 的对应记录为准。日常使用构建脚本分区烧录。
- 完整镜像：`release/mosicrokey-v0.1.9-esp32s31.bin`，16 MiB，烧录偏移 0x0；附带 SHA256、分段清单、ELF 和构建配置。
- 完整镜像包含空白 NVS 区域，会清除现有配网及模型配置；日常分区烧录使用构建脚本，保留 NVS。TF 游戏文件不在 Flash 镜像内。
- 已核对 COM8、ESP32-S31 rev0.0、16 MiB Flash 和 16 MiB PSRAM，分区烧录保留现有 NVS 配网及模型配置。

确认实际目标板 COM 口后，使用 `./scripts/build.ps1 -Flash -Port COM实际编号`。接线、分区烧录和完整镜像烧录步骤见[开发指南](docs/development-guide.md)。

UART 使用 115200、8N1、UTF-8、LF，固件不回显输入、不保存命令历史。常用命令：

```text
help
status
audio_status
game_selftest
wifi
agent_config {"url":"https://你的服务地址/v1","model":"支持工具调用的模型名","key":"你的密钥"}
demo
page games
gameask 给贪吃蛇增加逐步加速，并保存为新版本
cancel
remount
page inputs
input_status
page display
encoder 1
preview_demo
page pc
frame
game_source preview-snake
```

小智负责唯一的 Wi-Fi 初始化和标准激活链路。ESPClaw 使用 OpenAI 兼容 Chat Completions API 的真实请求及工具循环。两项服务驻留，前台独占音频、按键与画布；Agent 可跨页面后台创作，运行中的游戏保持启动时的版本。

智谱基址为 `https://open.bigmodel.cn/api/paas/v4`。当前设备采用免费 `glm-4-flash-250414`，已实测普通回复与 Snake 原生工具发布；前轮免费 `glm-4.7-flash` 也执行过游戏工具，但出现较多限流和超时。后端只对明确的 429/503 做最多两次可取消重试，共享原超时预算；迟到的成功响应也会丢弃，最终错误如实显示。游戏已发布但模型总结失败时同时显示发布结果。密钥和 Wi-Fi 密码仅通过不回显的配置命令保存至设备 NVS，不写进源码或发布镜像。

创作和修改使用 `gameask 需求`：只有真实发布工具成功才算完成，模型声明不能代替发布。普通对话或候选源码分析使用 `ask 文字`。USB 试用可发送 `gameask 实际调用game_preview生成贪吃蛇临时预览`，在游戏库确认启动，以 `key` 命令注入输入。`frame` 导出真实 VM 画布，配合 `scripts/capture-frame.py` 转为 PNG；`scripts/export-game-source.py` 可读取实际游戏源码并在电脑归档。预览不写 TF，重启后丢失；TF 安装仍需真实 TF 卡。

首版提供语音、串口创作、八键/旋钮导航、无声游戏及电脑串口控制。完整键盘、直接 USB HID、蓝牙音箱、无线控制器与通用存档留待后续。

## 文档与验证

- [实施状态与剩余验收](docs/implementation-status.md)
- [2026-10-01 验证记录](docs/validation/2026-10-01/README.md)
- [屏幕与输入接线](docs/tft-input-guide.md)、[当前共享 UI 截图](docs/validation/tft-input-2026-10-02/evidence/ui/contact-sheet.png)
- [板级约束](board-contract.md)、[架构](docs/architecture.md)、[游戏 API](docs/game-api.md)
- [需求](docs/product-requirements.md)、[验证计划](docs/verification-plan.md)
- [依赖与许可证](docs/dependencies-and-licenses.md)、[上游补丁](patches/README.md)

构建使用当前目录内的源码快照，不依赖上一段对话所在目录。首次解析组件仍需 SDK、组件仓库或本机缓存。原设计文档保存在 `docs/history/2026-09-29`。


