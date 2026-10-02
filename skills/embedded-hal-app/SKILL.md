---
name: embedded-hal-app
description: "为 ESP-IDF、STM32 和 FreeRTOS 项目编写或简化 HAL 驱动、APP 业务和 Task 调度。采用直接厂商 SDK 调用与大驼峰加下划线命名，适用于用户希望减少可移植驱动的指针抽象、提高人工阅读和 AI 编码可维护性的场景。"
---

# 嵌入式架构师：HAL → APP，Task 负责调度

目标：读者沿着函数名就能找到硬件实现、业务决策和运行任务。针对当前产品写清楚代码；发生实际平台迁移时再替换 HAL 实现。

本规范取代旧 BSP–APP–TASK skill 的强制函数指针、硬件绑定放 APP、一次/二次业务划分。用户明确要求的架构优先。参考依据和旧版取舍见 [references/design-decisions.md](references/design-decisions.md)；需要代码范式时读 [references/coding-example.md](references/coding-example.md)。

## 分层与目录

```text
hardware/hal_key.c/.h       GPIO、去抖、按键事件
hardware/hal_display.c/.h   SPI、控制器命令、DMA、背光
hardware/hal_storage.c/.h   SDMMC、挂载、设备状态
app/app_navigation.c/.h     页面与前台资源状态机
app/app_game.c/.h           创作、安装、启动、退出流程
task/key_task.c/.h          等待/周期扫描、投递输入
task/game_task.c/.h         帧调度、调用游戏业务
config/board_config.h       板卡引脚、外设实例与频率
config/task_config.h        栈、优先级、周期、核分配
main.c                     App_System_Init 与启动入口
```

已有 ESP-IDF component 或 C++ 项目保留组件目录与 `.cc/.hpp` 后缀，在组件内使用 `hardware/`、`app/`、`task/`；不要为了目录整齐搬动上游源码或新增多个空组件。

| 层 | 实际职责 | 允许依赖 |
|---|---|---|
| HAL | 外设初始化、读取/写入、协议时序、DMA/ISR、失败清理 | 本层、板级配置、厂商 SDK、必要 RTOS 同步 |
| APP | 业务用例、状态机、多外设协同、错误策略、服务结果 | HAL、业务数据、算法、真实服务模块、必要队列/锁 |
| Task | 等待事件、按周期执行、调用 APP、记录调度状态 | APP、RTOS、任务配置；采集/刷新任务可直接调用 HAL |

HAL 不依赖 APP 或页面。APP 不直接操作寄存器、GPIO/I²C/SPI/UART 句柄。Task 中不堆放菜单、游戏规则和业务状态转移。纯算法保持无硬件和 RTOS 依赖，可在主机测试。

## 命名：保留大驼峰 + 下划线

每个语义单词首字母大写，以 `_` 分隔；缩写用 `Hal`、`Uart`、`Wifi`、`Pid`、`Ui`。厂商 API、协议字段和上游 override 保留原名。

| 对象 | 命名示例 |
|---|---|
| HAL 函数 | `Hal_Key_Init`、`Hal_Key_Read`、`Hal_Display_Flush`、`Hal_Storage_Mount` |
| APP 业务 | `Page_Navigate`、`Game_Start`、`Game_Update_Process`、`Agent_Request_Submit` |
| 系统组装 | `App_System_Init`、`App_System_Run` |
| Task 入口 / 创建 | `Key_Scan_Task` / `Key_Scan_Task_Create` |
| 私有函数 | `Display_Send_Command`、`Key_Update_State`，文件内 `static` |
| 文件 | `hal_key.c`、`app_navigation.c`、`key_task.c`，全小写下划线 |
| C 类型 / 变量 | `key_event_t`、`display_config_t` / `current_mode`、`last_wake` |
| 宏 / 枚举常量 | `KEY_SCAN_PERIOD_MS`、`KEY_ID_CONFIRM`、`GAME_STATE_RUNNING` |

APP 函数以对象或业务开头，不机械增加 `App_`；系统组装保留 `App_`。原工程 HAL 已使用 `Key_Init` 时可继续保持，单一模块避免混用两套前缀；新项目默认 `Hal_`，防止 HAL 与业务同名。C++ 类方法也按此命名，但不重命名第三方虚函数。

## 直接驱动与必要封装

- 一个产品只有一种驱动实现时，HAL 直接调用 SDK。不要增加 `Gpio_Write`/`Pwm_Write` 函数指针表、`void *port`、每外设接口基类、注入工厂或重复转发层。
- SDK 的有类型句柄、缓冲区指针、配置结构体、RTOS 任务参数是正常用法。减少抽象不等于禁止指针。
- 单实例外设的句柄和状态放 `.c/.cc` 的 `static`/匿名命名空间；实际多实例需求才增加有类型实例参数。
- ISR、驱动完成通知、第三方要求的回调可以保留。回调在 HAL 中做最少同步，业务在任务上下文执行。
- 已有真实服务边界、第三方 Board/Codec 虚函数、确实用于错误注入的测试接口可保留，并在架构文档写明理由；不要把它们扩散到所有硬件驱动。
- 配置只集中一处。引脚/总线放 board config；任务参数放 task config；业务状态与硬件私有句柄不要全部塞入公开头文件。

## 业务与调度

APP 可以写状态机、组合流程和多外设协同。按“这个函数完成什么业务”划分，不按调用了几次 HAL 分层。长流程在专用工作任务执行，提供取消、结果与超时；周期业务拆成可重复调用的 `Xxx_Update_Process`。

Task 保持易读：等待 → 调用 APP → 等待。接收和显示任务可直接通过 HAL 收字节、读按键或刷新像素；不为禁止 Task include HAL 而写没有意义的 APP 包装。

头文件说明对外函数的调用上下文、是否阻塞、最大等待或超时来源、线程安全和缓冲区生命周期。允许工作任务中的有界存储/网络等待；不要在音频、UI、ISR 中执行模型等待或长时间存储操作。

## RTOS 与资源的实用约束

- 优先单任务拥有可变资源；跨任务使用队列、受锁保护快照或原子值。`volatile` 不能替代并发同步。
- 命令队列满时返回忙碌；不能悄悄清空业务命令。仅最新状态/帧可合并。
- 取锁失败返回超时/忙碌，不得继续操作，也不得释放未取得的锁。避免持有 UI/状态锁执行网络、长存储和回调。
- 初始化创建队列/锁，成功后再启动使用它们的任务；检查每次创建结果，部分启动失败有明确处理。ESP-IDF 的 `app_main` 已在调度器中运行，不调用 `vTaskStartScheduler`。
- 周期任务使用 `vTaskDelayUntil`；事件任务用阻塞接收，禁止空转。优先级依据实时期限，不能照搬“控制一律最高、显示一律最低”。
- ESP-IDF 的创建栈参数是字节；上游 FreeRTOS 通常是 `StackType_t` 个数。水位单位核对平台后再换算，栈目标留至少 30% 余量。
- ISR 使用允许的 `FromISR` API，不做日志、动态分配、业务流程和等待。厂商回调若通过返回值请求调度，不重复手动 yield。
- DMA 缓冲遵循目标芯片内存能力和驱动生命周期；超时不代表 DMA 已停止，不能立刻释放或修改仍在传输的缓冲。
- 外设失败返回原始原因。SD 挂载失败不默认格式化，NVS 初始化失败不默认擦除。初始化失败释放已取得资源，支持后续明确重试。

## 人与 Codex 的阅读路径

函数应能从命名直接判断职责；上下文和单位写在参数/字段名中，例如 `timeout_ms`、`stack_bytes`。四空格缩进，函数和控制流采用换行大括号。注释用简短中文解释时序、限制和原因；可用 `//=====显示初始化=====` 分组，不要求每行注释。

私有类型、常量和必要 `#include` 留在实现文件；公开头文件只展示调用者需要的内容。避免巨型通用 context、布尔参数组合、模板框架和为单行 SDK 再包数层的函数。使用已有 C/C++ 和 RTOS 机制，不为统一日志格式重写厂商日志库。

## 实施与验收

先读实际工程、板级资料、现有命名和用户指明的范例，画出简短调用链。列清每个硬件资源与状态的唯一拥有者、调用上下文及成功标准，再实现。

重构沿一条真实链路完成：HAL 实现 → APP 调用 → Task 接入 → CMake → 验证。删除被替代的旧入口，避免“新规范 + 旧实现”双轨长期存在。不要凭猜测补齐未连接外设的成功结果。

检查清单：

1. 是否能从 APP 函数跳到唯一 HAL 实现，HAL 内无业务依赖？
2. 是否存在只转发一行却没有边界价值的接口、指针表、工厂或文件？
3. 业务状态机是否在 APP，任务循环是否可快速读懂？
4. 失败、取消、队列满、超时、重试、资源清理和取锁失败是否可解释？
5. 构建和已有回归是否通过；新增测试是否验证真实行为或风险？
6. 实板证据是否记录固件版本、端口和日志，明确区分未连接外设？

迁移报告说明实际完成范围和暂时保留的边界，不把编译通过等同于硬件通过，也不为了满足本 skill 擅自升级固定 SDK 或第三方依赖。
