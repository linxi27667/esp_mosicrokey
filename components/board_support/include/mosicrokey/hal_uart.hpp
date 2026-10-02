#pragma once
#include <esp_err.h>
#include <cstddef>
#include <cstdint>

namespace mosicrokey::hal
{
// 启动时配置 UART0 RX；TX 继续由 ESP-IDF stdout/日志拥有，不创建第二套输出。
esp_err_t Hal_Uart_Init();
// 仅 Console_Task 调用；最多等待 timeout_ms。received=0 表示正常读超时。
esp_err_t Hal_Uart_Read(uint8_t* data, size_t capacity, size_t& received, uint32_t timeout_ms);
} // namespace mosicrokey::hal
