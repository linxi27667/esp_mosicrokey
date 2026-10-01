# MosiCrokey v0.1.9

源码仓库：[linxi27667/esp_mosicrokey](https://github.com/linxi27667/esp_mosicrokey)，私有，默认分支 main。开发与验收变更通过 Git 提交记录，见[Git 管理流程](docs/git-workflow.md)。

当前目录是独立的 ESP-IDF 工程：真实小智组件、真实 ESPClaw Agent、TF 游戏版本库、受限 Lua 运行时和共享 LVGL 页面已实现。2026-10-01 已在 COM8 的 ESP32-S31 上烧录、联网并完成真实智谱请求和 Agent 贪吃蛇生成；当前仅连接 USB，游戏采用明确标注的临时内存预览。

1178 项主机检查及共享页面截图通过，实际设备导出的 AI 贪吃蛇另通过 32 项玩法检查。小智仍等待账号激活；语音完整对话、实体屏/按键/TF 持久化尚未实测。两小时稳定性以[实板验证记录](docs/validation/hardware-2026-10-01/README.md)中对应固件的最终日志为准。

## 快速使用

在当前项目根目录的 PowerShell 执行：

```powershell
./scripts/build.ps1
./scripts/test-host.ps1
./scripts/package-release.ps1
```

- SDK：本机 ESP-IDF 6.1.0，target 为 `esp32s31`。
- 开发板：ESP32-S31-Function-CoreBoard-1；显示 ST7735、160×128 横屏。
- 版本：小智 `ed5cd7ba0011dc06fefd0762790857268c41b430`，ESPClaw `74b18700a1d6c40de472bfc71c19e49356ca1cc0`，LVGL 9.5.0，Lua 5.5.0~7。
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
frame
game_source preview-snake
```

小智负责唯一的 Wi-Fi 初始化和标准激活链路。ESPClaw 使用 OpenAI 兼容 Chat Completions API 的真实请求及工具循环。两项服务驻留，前台独占音频、按键与画布；Agent 可跨页面后台创作，运行中的游戏保持启动时的版本。

智谱基址为 `https://open.bigmodel.cn/api/paas/v4`。当前设备采用免费 `glm-4-flash-250414`，已实测普通回复与 Snake 原生工具发布；前轮免费 `glm-4.7-flash` 也执行过游戏工具，但出现较多限流和超时。后端只对明确的 429/503 做最多两次可取消重试，共享原超时预算；迟到的成功响应也会丢弃，最终错误如实显示。游戏已发布但模型总结失败时同时显示发布结果。密钥和 Wi-Fi 密码仅通过不回显的配置命令保存至设备 NVS，不写进源码或发布镜像。

创作和修改使用 `gameask 需求`：只有真实发布工具成功才算完成，模型声明不能代替发布。普通对话或候选源码分析使用 `ask 文字`。USB 试用可发送 `gameask 实际调用game_preview生成贪吃蛇临时预览`，在游戏库确认启动，以 `key` 命令注入输入。`frame` 导出真实 VM 画布，配合 `scripts/capture-frame.py` 转为 PNG；`scripts/export-game-source.py` 可读取实际游戏源码并在电脑归档。预览不写 TF，重启后丢失；TF 安装仍需真实 TF 卡。

首版提供语音、串口创作、方向/确认/返回按键及无声游戏。完整键盘、USB HID、蓝牙音箱、无线控制器与通用存档留待后续。

## 文档与验证

- [实施状态与剩余验收](docs/implementation-status.md)
- [2026-10-01 验证记录](docs/validation/2026-10-01/README.md)
- [共享 UI 截图](docs/validation/hardware-2026-10-01/evidence/ui-v019/contact-sheet.png)
- [板级约束](board-contract.md)、[架构](docs/architecture.md)、[游戏 API](docs/game-api.md)
- [需求](docs/product-requirements.md)、[验证计划](docs/verification-plan.md)
- [依赖与许可证](docs/dependencies-and-licenses.md)、[上游补丁](patches/README.md)

构建使用当前目录内的源码快照，不依赖上一段对话所在目录。首次解析组件仍需 SDK、组件仓库或本机缓存。原设计文档保存在 `docs/history/2026-09-29`。


