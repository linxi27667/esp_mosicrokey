# MosiCrokey 开发约定

项目采用 [HAL–APP 规范](skills/embedded-hal-app/SKILL.md)。涉及自有驱动、业务或任务时先读该文件。用户旧 RTOS skill 仅作为命名历史，不再要求 BSP 函数指针或把状态机放 Task。

- HAL 直接调用 ESP-IDF；APP 编写业务/状态机；Task 负责等待与调度。
- 自有新函数使用 `Hal_Display_Init`、`Page_Navigate`、`Game_Update_Process`、`Xxx_Task_Create` 风格；厂商 API 与上游 override 保留原名。
- 板级参数在 `board_config.hpp`，项目任务参数在 `task_config.hpp`。SDK 固定 v6.1 / `esp32s31`，不套用 S3 引脚约束。
- 保留 `compoment` 拼写及固定上游快照/补丁；一套 Wi-Fi、一套音频、一套 LVGL、一个 app_main/Board 工厂。
- 服务边界的现有接口用于真实上游与主机故障注入；不要为单实例硬件新增接口基类、注入表或工厂。
- 构建 `scripts/build.ps1`，回归 `scripts/test-host.ps1`；新 skill 用 `python -X utf8 .../quick_validate.py skills/embedded-hal-app` 校验。
- 不自动格式化 TF、擦除 NVS 或在源码/日志/提交中写入凭证。已发布标签不可移动。
- 每次实板结论附固件版本、端口、哈希与日志。用户已接入 TFT，报告黑屏；型号/供电和实体显示仍待确认，不能将 SPI 提交成功视为点亮。TF/按键/旋钮的接线未确认，小智账号激活仍待完成。
