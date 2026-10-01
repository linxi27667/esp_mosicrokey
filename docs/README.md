# 文档地图

当前基线是 v0.1.9 独立 S31 工程，已在 COM8 实板进行 USB 范围的联网与 AI 游戏闭环。文档按实现更新；原初始设计保存在 history/2026-09-29。

| 文档 | 内容 |
|---|---|
| [实施状态](implementation-status.md) | 已实现与仍需实板/服务验收的阶段 |
| [产品需求](product-requirements.md) | 首版范围与需求编号 |
| [板级约束](../board-contract.md) | 默认 GPIO、电气约束与官方资料 |
| [架构](architecture.md) | 组件、接口、任务、资源与上游适配 |
| [开发指南](development-guide.md) | 本机环境、构建、烧录和串口操作 |
| [游戏 API](game-api.md) | 包格式、工具、Lua API 和沙箱限制 |
| [验证计划](verification-plan.md) | 验收步骤与证据要求 |
| [验证记录](validation/2026-10-01/README.md) | 已执行测试、截图、日志和固件哈希 |
| [USB 实板记录](validation/hardware-2026-10-01/README.md) | 当前固件、真实 AI Snake、设备画布、持续测试与未完成硬件验收 |
| [依赖与许可证](dependencies-and-licenses.md) | 版本、来源、字体与第三方许可 |
| [模块索引](modules/README.md) | 双 AI 与游戏模块的当前约定 |
| [Mosaico 参考](references/esp-mosaico.md) | 初期调研背景；本项目自行实现 TF Lua 动态运行时 |
| [上游补丁](../patches/README.md) | 已应用的产品适配改动 |

决策记录目录保留用于后续显著架构调整。当前实施边界以需求、架构和游戏 API 为准。





