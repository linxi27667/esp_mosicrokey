#include "mosicrokey/hal_uart.hpp"
#include "mosicrokey/board_config.hpp"
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <climits>

namespace mosicrokey::hal
{
namespace
{
bool g_ready = false;
}

esp_err_t Hal_Uart_Init()
{
    if (g_ready)
    {
        return ESP_OK;
    }
    // 外部已安装驱动时拒绝接管，避免破坏别的模块配置。
    if (uart_is_driver_installed(UART_NUM_0))
    {
        return ESP_ERR_INVALID_STATE;
    }
    uart_config_t config{};
    config.baud_rate = board::config::uart_baud;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    esp_err_t error = uart_param_config(UART_NUM_0, &config);
    if (error == ESP_OK)
    {
        error = uart_set_pin(UART_NUM_0, board::config::uart_tx, board::config::uart_rx, UART_PIN_NO_CHANGE,
                             UART_PIN_NO_CHANGE);
    }
    if (error == ESP_OK)
    {
        error = uart_driver_install(UART_NUM_0, 8192, 0, 0, nullptr, 0);
    }
    g_ready = error == ESP_OK;
    return error;
}

esp_err_t Hal_Uart_Read(uint8_t* data, size_t capacity, size_t& received, uint32_t timeout_ms)
{
    received = 0;
    if (!g_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (!data || !capacity || capacity > INT_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int size = uart_read_bytes(UART_NUM_0, data, capacity, pdMS_TO_TICKS(timeout_ms));
    if (size < 0)
    {
        return ESP_FAIL;
    }
    received = static_cast<size_t>(size);
    return ESP_OK;
}
} // namespace mosicrokey::hal
