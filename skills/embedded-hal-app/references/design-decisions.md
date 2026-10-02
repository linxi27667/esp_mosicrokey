# 设计来源与旧版取舍

核对日期：2026-10-02。本规范是针对当前产品开发的选择，不声称某家公司只有一种架构或这是一套通用最优方案。

## 实际参考

- 用户旧版：`E:/MCU/BSP/skills/嵌入式架构师(RTOS版)/SKILL.md`，SHA256 `2be8f103dae748ca107f6e36d082c4de9f95e6e384b8b8a4128059c37db7f0b3`。与 Codex 安装副本不同，以用户本次指明版本为取舍依据。
- 本机直接 HAL 范例：`E:/MCU/esp32/ESP_BSP_IN_QIANSAI/demo/main/hardware/hal_key.c`、`hal_uart.c`、`main/task/uart_task.c`。学习直接 SDK 调用、集中配置、类型化数据与任务入口；不照搬该 ESP32-S3 工程的引脚和时序。
- [Espressif ESP-BSP 使用说明](https://github.com/espressif/esp-bsp/blob/master/docu/how_to_use.md)：提供直接初始化、可选配置结构体、显示/存储/音频 API，以及内部管理的 SDK 资源。借鉴“常用路径直接调用，需要时再配置”，不要求把全部 GPIO 操作抽象成用户函数指针。
- [ST 官方 UART HAL 实现](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Src/stm32f4xx_hal_uart.c)：有类型句柄、明确的轮询/中断/DMA 操作。借鉴明确参数、状态与超时，不复制 STM32 的命名或寄存器到 ESP-IDF。
- [Zephyr 官方消息队列说明](https://docs.zephyrproject.org/latest/kernel/services/data_passing/message_queues.html)：线程/ISR 传递有界消息，处理队列满与不能等待的上下文。借鉴通信责任；不要把文档中的“清空队列”例子当成业务命令通用策略。

上述公开实现支持部分机制选择；HAL/APP/Task 的组合、命名和裁剪是本规范的工程判断。

## 保留、调整与删除

| 旧规则 | 新规则与原因 |
|---|---|
| 大驼峰 + 下划线 | 保留，硬件 `Hal_`，业务对象开头，入口 `Xxx_Task` |
| BSP 禁止 SDK，APP 唯一包含硬件头 | HAL 直接依赖 SDK，APP 通过 HAL 工作，硬件实现集中 |
| 每个设备结构包含 GPIO/PWM/编码器函数指针 | 只有实际多平台运行时选择需求才考虑；单产品直接调用 |
| APP 禁止状态机和多外设协同 | APP 拥有业务状态机与完整用例，Task 保持调度职责 |
| 所有类型、宏、include 都放公开头 | 私有实现留 `.c/.cc`，减少公开依赖和认知负担 |
| Task 绝不 include BSP | 采集和刷新 Task 可直接调用 HAL，避免无意义转发 |
| 跨任务绝不共享状态 | 优先单一拥有者；允许受锁保护快照和明确原子数据 |
| 原子业务函数绝不阻塞 | 定义上下文与等待上限，长操作放工作任务 |
| 拿锁后直接操作，无论返回值 | 仅取锁成功才操作/释放；超时上报，原示例不能直接复制 |
| ISR 标志一律 volatile | ISR 同步依平台要求，volatile 不提供原子性或线程安全 |
| 所有 MCU 手动启动调度器 | ESP-IDF 已启动；其他平台依据真实启动流程 |
| 固定优先级表与固定日志格式 | 根据期限分配任务，保留厂商日志与真实栈单位 |

## MosiCrokey 的边界

硬件 HAL 为显示、存储、按键、串口、音频控制 I²C。小智音频流和 ES8311 codec 继续使用固定上游驱动及其任务，不复制另一套 I²S/codec 生命周期。板卡、Display、Codec 的 override 是上游真实接口要求。

现有 `IXiaozhiService`、`IAgentService`、`IGameLibrary`、`IGameRuntime` 位于服务边界，用于隔离真实上游及主机故障注入，保留它们不意味着为每个 GPIO 再建抽象。硬件 API 不返回这些接口，也不依赖 APP。
