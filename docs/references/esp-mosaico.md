# ESP-Mosaico 参考调研

- 来源：乐鑫 ESP-Mosaico 官方网站及其公开 GitHub 仓库
- 核验日期：2026-09-29
- 用途：借鉴 AI 创作、设备应用与游戏开发流程；不视为 MosiCrokey 已选硬件或必须照搬的实现

## 1. ESP-Mosaico 的产品工作流

乐鑫官方指南将体验分为两种相关路径：

- **ChatCoding**：用户用自然语言提出需求，设备端 Agent 生成并运行代码，用户观察效果后继续对话修改；完成的技能可以保存、复用和继续完善。
- **VibeCoding**：开发者在 PC 上使用 AI 编码 Agent、模拟器和实体板共同开发较完整的应用；先用模拟器迭代，再构建和安装到设备。

这是 MosiCrokey 可参考的交互和创作闭环：描述想法 → 生成/构建 → 预览或运行 → 反馈修改 → 保存成果 → 再次打开。

## 2. ESP-Mosaico 游戏开发路径

公开的 `esp-mosaico-vibe` 文档将游戏作为 C 应用开发：使用 Raylib Lite Engine、Host 模拟器以及 BSP 游戏示例；PC 模拟器与设备共享游戏逻辑，验证后再构建并安装到设备。其公开示例包括 Sky Hop、Tower Defense 和 Shooter。

这一方案值得参考的部分：

- 为 Agent 提供明确的游戏模板、引擎 API 和项目目录约定。
- Host 模拟器先验证视觉、输入和玩法，再做实体板测试。
- 共享设备端与 Host 的游戏逻辑，支持可重复的输入回放/状态验证。
- 游戏有清晰的创建、构建、预览、安装和回归流程。

## 3. 与 MosiCrokey 目标的差异

MosiCrokey 的已确认目标比当前公开 Mosaico 工作流多出两项约束：

1. 期望 **设备上的 ESPClaw Agent 自己参与游戏创作**，而非仅依赖 PC 上的 VibeCoding Agent 生成完整 C 固件应用。
2. 游戏及其资源要 **存放在 TF 卡**，设备上提供游戏入口以发现、选择和启动；换言之，游戏成果需要能作为应用数据/游戏包独立保存和加载。

本次核验的公开 Mosaico 游戏文档描述的是 Agent 在 PC 端构建 C 游戏应用，并通过设备 CLI 安装/更新固件应用；没有确认其公开游戏方案将游戏包安装到 TF 卡并由常驻固件动态加载。因此，Mosaico 可以作为创作体验、游戏 API、Host 模拟器和迭代流程的参考，但不能据此认为 TF 卡游戏管理或设备端 C 编译已实现。

为满足 MosiCrokey 的目标，需要额外设计创作结果格式与执行路径。候选方向包括受限脚本/数据驱动游戏包、设备端解释器、PC 构建后导出的兼容包，或组合路径。尚未决定具体技术。

## 4. 硬件与工具链兼容性提醒

ESP-Mosaico 本身是一块 ESP32-S31 开发设备，其公开 BSP 仓库记录了该设备的板载外设。但 MosiCrokey 的开发板尚未选定，不能直接套用 Mosaico 的引脚、屏幕、音频或存储配置。

当前公开 `esp-mosaico-bsp` 文档列出的依赖是 ESP-IDF 6.2 或更新版本；`esp-mosaico-vibe` 工作区则固定到一个 ESP-IDF master 提交。MosiCrokey 当前本机基线为 ESP-IDF v6.1，因此 Mosaico BSP/游戏堆栈不能被假定可直接用于当前环境。移植、版本升级或仅借鉴设计需在集成前做兼容验证。

MosiCrokey 要求 TF 卡；本次查阅的 Mosaico BSP 说明的是自身板载外设和 SPI NAND 存储，没有据此确认 Mosaico 参考方案能提供 TF 游戏库。本项目仍须自行确定 TF 卡硬件接口、文件系统和读写/移除策略。

## 5. 对 MosiCrokey 的应用

当前可采纳为产品方向的参考原则：

- AI 创建功能要形成能运行、能观察结果、能反复修改的闭环。
- 游戏必须有面向目标硬件的引擎/API 契约和可重复预览验证路径。
- Agent 输出不能止于聊天文本；生成物必须成为设备可发现和可启动的游戏。
- 游戏内容与主固件解耦保存到 TF 卡，更新或删除游戏不应要求重刷整套固件（具体实现待设计）。

以上是 MosiCrokey 的目标约束和参考原则，不代表已决定采用 Raylib Lite、Mosaico BSP、其 CLI、分区布局或许可证组合。

## 6. 上游资料

- [ESP-Mosaico 官方网站](https://mosaico.espressif.com/zh/)
- [ESP-Mosaico ChatCoding 指南](https://mosaico.espressif.com/zh/guide/chat-coding/)
- [ESP-Mosaico VibeCoding 指南](https://mosaico.espressif.com/zh/guide/vibe-coding/)
- [ESP-Mosaico Vibe 工作区](https://github.com/esp-mosaico/esp-mosaico-vibe)
- [ESP-Mosaico 游戏开发文档](https://github.com/esp-mosaico/esp-mosaico-vibe/blob/main/docs/game-development.md)
- [ESP-Mosaico BSP](https://github.com/esp-mosaico/esp-mosaico-bsp)
- [Raylib Lite Engine](https://github.com/espressif2022/raylib-lite-engine)


