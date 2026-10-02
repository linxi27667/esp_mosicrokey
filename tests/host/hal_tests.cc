#include "mosicrokey/hal_key.hpp"
#include "mosicrokey/hal_uart.hpp"
#include "mosicrokey/board_config.hpp"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <array>
#include <iostream>
#include <cstdlib>
#include <climits>

using namespace mosicrokey;
namespace {
unsigned checks = 0, gpio_calls = 0, uart_calls = 0;
esp_err_t gpio_error = ESP_FAIL, uart_error = ESP_FAIL;
bool uart_installed = false;
int uart_result = 0;
std::array<int, 64> levels;
uint32_t read_timeout = 0;
void Check(bool valid, const char* name)
{
    ++checks;
    if (!valid)
    {
        std::cerr << "FAIL: " << name << '\n';
        std::exit(1);
    }
}
}

esp_err_t gpio_config(const gpio_config_t* config)
{
    ++gpio_calls;
    uint64_t expected = 0;
    for (int pin : board::config::keys) expected |= 1ULL << pin;
    Check(config->pin_bit_mask == expected && config->mode == GPIO_MODE_INPUT &&
          config->pull_up_en == GPIO_PULLUP_ENABLE, "S31 eight keys plus encoder press and pull-ups");
    return gpio_error;
}
int gpio_get_level(gpio_num_t pin)
{
    return levels[pin];
}
bool uart_is_driver_installed(int port)
{
    Check(port == UART_NUM_0, "console owns UART0 only");
    return uart_installed;
}
esp_err_t uart_param_config(int port, const uart_config_t* config)
{
    ++uart_calls;
    Check(port == UART_NUM_0 && config->baud_rate == 115200 && config->data_bits == 8 &&
          config->parity == UART_PARITY_DISABLE && config->stop_bits == UART_STOP_BITS_1,
          "UART 115200 8N1 config");
    return uart_error;
}
esp_err_t uart_set_pin(int, int tx, int rx, int rts, int cts)
{
    Check(tx == 58 && rx == 59 && rts == UART_PIN_NO_CHANGE && cts == UART_PIN_NO_CHANGE,
          "S31 console pins and no hardware flow control");
    return ESP_OK;
}
esp_err_t uart_driver_install(int, int rx, int tx, int queue, void*, int)
{
    Check(rx == 8192 && tx == 0 && queue == 0, "bounded RX and existing stdout TX");
    uart_installed = true;
    return ESP_OK;
}
int uart_read_bytes(int, void* data, uint32_t length, TickType_t timeout)
{
    read_timeout = timeout;
    if (uart_result == 1 && length) *static_cast<uint8_t*>(data) = 'x';
    return uart_result;
}

int main()
{
    levels.fill(1);
    std::array<hal::key_event_t, board::config::keys.size()> events;
    size_t count = 123;
    bool pressed = false;
    Check(hal::Hal_Key_Read(0, pressed) == ESP_ERR_INVALID_STATE, "uninitialized key rejected");
    Check(hal::Hal_Key_Scan(0, events.data(), events.size(), count) == ESP_ERR_INVALID_STATE && !count,
          "uninitialized scan clears output");
    Check(hal::Hal_Key_Init() == ESP_FAIL, "GPIO failure propagated");
    gpio_error = ESP_OK;
    Check(hal::Hal_Key_Init() == ESP_OK, "GPIO retry succeeds");
    unsigned before = gpio_calls;
    Check(hal::Hal_Key_Init() == ESP_OK && before == gpio_calls, "key init idempotent");
    Check(hal::Hal_Key_Read(board::config::keys.size(), pressed) == ESP_ERR_INVALID_ARG, "bad key ID rejected");
    Check(hal::Hal_Key_Scan(0, nullptr, events.size(), count) == ESP_ERR_INVALID_ARG && !count,
          "null event buffer rejected");
    Check(hal::Hal_Key_Scan(0, events.data(), 5, count) == ESP_ERR_INVALID_ARG,
          "short event buffer rejected without consuming input");
    auto scan = [&](uint32_t now) {
        Check(hal::Hal_Key_Scan(now, events.data(), events.size(), count) == ESP_OK, "scan succeeds");
    };
    scan(0); Check(!count, "idle emits no events");
    levels[board::config::keys[0]] = 0;
    scan(5); scan(24); Check(!count, "press under 20 ms is filtered");
    scan(25); Check(count == 1 && events[0].id == 0 && events[0].pressed, "stable press emitted once");
    scan(45); Check(!count, "held press not repeated");
    levels[board::config::keys[0]] = 1;
    scan(50); levels[board::config::keys[0]] = 0;
    scan(55); scan(80); Check(!count, "short release bounce not emitted");
    levels[board::config::keys[0]] = 1;
    scan(85); scan(105); Check(count == 1 && !events[0].pressed, "stable release emitted once");
    for (int pin : board::config::keys) levels[pin] = 0;
    scan(110); scan(130); Check(count == board::config::keys.size(), "simultaneous nine presses preserved");
    for (unsigned i = 0; i < count; ++i) Check(events[i].id == i && events[i].pressed, "event IDs match hardware order");
    for (int pin : board::config::keys) levels[pin] = 1;
    scan(0xfffffffbu); scan(14); Check(!count, "wrapped 19 ms is still filtered");
    scan(15); Check(count == board::config::keys.size(), "debounce survives millisecond wrap at 20 ms");

    uint8_t byte = 0;
    size_t received = 99;
    Check(hal::Hal_Uart_Read(&byte, 1, received, 100) == ESP_ERR_INVALID_STATE && !received,
          "uninitialized UART rejected");
    uart_installed = true;
    Check(hal::Hal_Uart_Init() == ESP_ERR_INVALID_STATE && !uart_calls, "foreign UART driver not taken over");
    uart_installed = false;
    Check(hal::Hal_Uart_Init() == ESP_FAIL, "UART config failure propagated");
    uart_error = ESP_OK;
    Check(hal::Hal_Uart_Init() == ESP_OK, "UART retry succeeds");
    before = uart_calls;
    Check(hal::Hal_Uart_Init() == ESP_OK && before == uart_calls, "UART init idempotent");
    Check(hal::Hal_Uart_Read(nullptr, 1, received, 100) == ESP_ERR_INVALID_ARG && !received,
          "null UART data rejected");
    Check(hal::Hal_Uart_Read(&byte, 0, received, 100) == ESP_ERR_INVALID_ARG,
          "zero UART capacity rejected");
    Check(hal::Hal_Uart_Read(&byte, size_t(INT_MAX) + 1, received, 100) == ESP_ERR_INVALID_ARG,
          "oversized UART read rejected");
    uart_result = 0;
    Check(hal::Hal_Uart_Read(&byte, 1, received, 100) == ESP_OK && !received && read_timeout == 100,
          "normal bounded UART timeout is not a failure");
    uart_result = -1;
    Check(hal::Hal_Uart_Read(&byte, 1, received, 100) == ESP_FAIL && !received, "UART read failure propagated");
    uart_result = 1;
    Check(hal::Hal_Uart_Read(&byte, 1, received, 100) == ESP_OK && received == 1 && byte == 'x',
          "UART byte returned without echo");
    std::cout << "PASS " << checks << " checks: actual HAL key debounce and UART ownership/errors\n";
}
