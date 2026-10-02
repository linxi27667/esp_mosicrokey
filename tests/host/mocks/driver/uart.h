#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <cstddef>
#include <cstdint>
constexpr int UART_NUM_0 = 0, UART_DATA_8_BITS = 8, UART_PARITY_DISABLE = 0;
constexpr int UART_STOP_BITS_1 = 1, UART_HW_FLOWCTRL_DISABLE = 0;
constexpr int UART_SCLK_DEFAULT = 0, UART_PIN_NO_CHANGE = -1;
struct uart_config_t
{
    int baud_rate = 0, data_bits = 0, parity = 0, stop_bits = 0, flow_ctrl = 0, source_clk = 0;
};
bool uart_is_driver_installed(int port);
esp_err_t uart_param_config(int port, const uart_config_t* config);
esp_err_t uart_set_pin(int port, int tx, int rx, int rts, int cts);
esp_err_t uart_driver_install(int port, int rx, int tx, int queue, void* handle, int flags);
int uart_read_bytes(int port, void* data, uint32_t length, TickType_t timeout);
