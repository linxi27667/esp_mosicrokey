# 双 AI 与资源焦点

小智与 ESPClaw 都驻留，业务通过 IXiaozhiService / IAgentService 使用；两项适配器不直接调用彼此。小智初始化唯一 Wi-Fi 栈，ESPClaw 复用网络。

小智页面取得前台音频焦点，离页立即关闭门控并在 Application::Schedule 中停止采集、音频测试、对话与播放。迟到 TTS、唤醒与解码检查同一门控。游戏独占方向/确认输入与画布，返回键由业务退出。

ESPClaw 请求编号保持跨页面有效，确认/串口 cancel 请求取消。忙碌时拒绝新请求、配置更改及 TF 重挂。App 的操作队列满时返回 Busy；状态合并，LVGL 仅在 UI 任务使用。

主页状态栏 W+/W- 表示 Wi-Fi，TF+/TF- 表示卡，X+/X- 表示小智前台音频焦点，A* / A. 表示 Agent 工作/空闲。小智页另外显示真实协议状态和文本，Agent 页显示工具进度与结果。

实现细节、任务参数与主机/设备证据边界见[架构](../architecture.md)和[验证计划](../verification-plan.md)。
