#pragma once
#include <esp_err.h>
#include <driver/i2c_master.h>

namespace mosicrokey::hal
{
// 启动时由唯一小智 Board 调用；不创建 I²S 或重复 codec 任务。
esp_err_t Hal_Audio_Bus_Init();
// 有类型的 SDK 句柄，借用给固定上游 ES8311 驱动，不转移所有权。
i2c_master_bus_handle_t Hal_Audio_Bus_Get();
} // namespace mosicrokey::hal
