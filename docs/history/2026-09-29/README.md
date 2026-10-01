# MosiCrokey

MosiCrokey 是一款面向对 AI 感兴趣的青少年的开源嵌入式设备项目。产品设想以键盘为载体，在键盘右侧集成屏幕，设备同时提供小智 AI 与 ESPClaw，并支持用 ESPClaw 创作游戏、保存到 TF 卡和从设备入口游玩。

项目负责人负责硬件设计，并对软件约束和架构作决策；软件通过逐步明确需求、接口和验收标准，再与 AI 协作实现。未经核实的规格和上游行为明确标记为待确认或待调研。

## 当前状态

当前处于需求、硬件和文档基线整理阶段。开发板、模组、存储配置及实际外设尚未确认，固件工程目录尚未建立。

## 已明确的产品目标

- 一份产品固件同时搭载小智 AI 与 ESPClaw；用户不需重新烧录即可进入两项功能。
- ESPClaw 可参与创作/修改设备可运行的游戏。
- 游戏程序和资源存入 TF 卡；设备能够发现并启动 TF 卡中的游戏。
- 参考乐鑫 [ESP-Mosaico 的 ChatCoding/VibeCoding 与游戏开发工作流](docs/references/esp-mosaico.md)，同时针对 MosiCrokey 增加 ESPClaw 设备端创作和 TF 游戏库要求。
- 评估蓝牙音箱及外接遥控器/摇杆作为游戏控制器的方案。

产品目标不代表功能已实现。游戏格式/运行时、双 AI 集成方式及同时运行策略仍须设计和验证。

## 芯片与软件基线

- 目标系列：ESP32-S31；ESP-IDF target 名称 `esp32s31`。
- 本机开发基线：ESP-IDF v6.1，路径 `E:\MCU\esp32\.espressif\v6.1\esp-idf`。
- 本机源码将 `esp32s31` 列为 preview target，并为该 target 选择 `IDF_ENV_BRINGUP`；目标板构建和功能验证尚无记录。
- 软件方向暂定 ESP-IDF + FreeRTOS。
- 乐鑫 ESP32-S31 预发布数据手册 v0.5 列出 Sv32 两级页表地址转换；该信息不单独证明通用 Linux/Ubuntu 的启动链和驱动支持。本项目当前不承诺 Linux。
- 当前公开 Mosaico BSP 要求 ESP-IDF 6.2 或更新版本，与本机 v6.1 基线有版本兼容待验证项。
- 具体板卡的 Flash/PSRAM、引脚、TF 卡接口、供电和外设须依据实际硬件资料确认。

## 文档入口

- [文档地图](docs/README.md)：文档分工和阅读顺序。
- [产品需求与范围](docs/product-requirements.md)
- [板级约束](board-contract.md)
- [系统架构](docs/architecture.md)
- [开发指南](docs/development-guide.md)
- [验证计划](docs/verification-plan.md)
- [ESP-Mosaico 参考调研](docs/references/esp-mosaico.md)
- [模块文档目录](docs/modules/README.md)
- [架构决策记录](docs/decisions/README.md)

## 开发约定

- 区分已确认事实、芯片级信息、设计提案、待调研和待确认事项，并注明依据。
- 不根据芯片或上游项目名称推断板卡资源、兼容性或许可证。
- 先明确模块职责和接口，再实现功能；不未经讨论扩大范围或引入复杂架构。
- 区分板上实时交互与依赖手机、局域网或云服务的能力，不默认大模型在 MCU 本地运行。
- 常规开发以 ESP-IDF 构建、烧录和日志调试为基线；具体板级步骤待目标板确定。


