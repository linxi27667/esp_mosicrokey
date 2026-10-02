#include "mosicrokey/hal_encoder.hpp"
#include "mosicrokey/board_config.hpp"
#include "driver/gpio.h"
#include <array>
#include <iostream>
#include <cstdlib>
using namespace mosicrokey;
namespace
{
unsigned checks = 0, config_calls = 0, uninstalls = 0, removes = 0;
std::array<int,64> levels;
std::array<void (*)(void*),64> handlers{};
esp_err_t config_error = ESP_FAIL, service_error = ESP_OK;
int fail_pin = -1;
void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
void Phase(unsigned phase)
{
    levels[board::config::encoder_a] = phase >> 1;
    levels[board::config::encoder_b] = phase & 1;
    handlers[board::config::encoder_a](nullptr);
}
int Read()
{
    int detents = 99;
    Check(hal::Hal_Encoder_Read(detents) == ESP_OK, "read succeeds");
    return detents;
}
}
esp_err_t gpio_config(const gpio_config_t* config)
{
    ++config_calls;
    Check(config->pin_bit_mask == ((1ULL<<0)|(1ULL<<3)) && config->pull_up_en == GPIO_PULLUP_ENABLE &&
          config->intr_type == GPIO_INTR_ANYEDGE, "S31 A/B pins, pull-ups, both edges");
    return config_error;
}
int gpio_get_level(gpio_num_t pin) { return levels[pin]; }
esp_err_t gpio_install_isr_service(int flags) { Check(!flags, "ISR flash-enabled service"); return service_error; }
void gpio_uninstall_isr_service() { ++uninstalls; }
esp_err_t gpio_isr_handler_add(gpio_num_t pin, void (*handler)(void*), void*)
{
    if (pin == fail_pin) return ESP_FAIL;
    handlers[pin] = handler;
    return ESP_OK;
}
esp_err_t gpio_isr_handler_remove(gpio_num_t pin) { ++removes; handlers[pin] = nullptr; return ESP_OK; }
int main()
{
    levels.fill(1);
    int detents = 99;
    Check(hal::Hal_Encoder_Read(detents) == ESP_ERR_INVALID_STATE && detents == 0, "uninitialized output zeroed");
    Check(hal::Hal_Encoder_Init() == ESP_FAIL, "GPIO error propagated");
    config_error = ESP_OK; service_error = ESP_FAIL;
    Check(hal::Hal_Encoder_Init() == ESP_FAIL, "ISR service error propagated");
    service_error = ESP_OK; fail_pin = board::config::encoder_b;
    Check(hal::Hal_Encoder_Init() == ESP_FAIL, "second handler failure propagated");
    Check(removes == 1 && uninstalls == 1 && !handlers[0], "owned service and first handler rolled back");
    service_error = ESP_ERR_INVALID_STATE;
    Check(hal::Hal_Encoder_Init() == ESP_FAIL && uninstalls == 1 && removes == 2, "borrowed service survives failure");
    fail_pin = -1;
    Check(hal::Hal_Encoder_Init() == ESP_OK, "retry with shared service succeeds");
    unsigned before = config_calls;
    Check(hal::Hal_Encoder_Init() == ESP_OK && before == config_calls, "init idempotent");
    Phase(2); Phase(0); Phase(1); Phase(3);
    Check(Read() == 1 && Read() == 0, "clockwise detent drained once");
    Phase(1); Phase(0); Phase(2); Phase(3);
    Check(Read() == -1, "counterclockwise direction");
    Phase(2); Phase(3); Phase(2); Phase(3);
    Check(Read() == 0, "contact bounce cancels");
    Phase(2); Phase(1); Phase(3);
    Check(Read() == 0, "illegal two-bit transition discards partial detent");
    Phase(2); Phase(0); Phase(1); Phase(3);
    Check(Read() == 1, "recovers after illegal transition");
    for(int i=0;i<1050;++i) { Phase(2); Phase(0); Phase(1); Phase(3); }
    Check(Read() == 1024, "bounded burst does not overflow");
    std::cout << "PASS " << checks << " checks: actual GPIO encoder ISR, rollback, bounce, direction and saturation\n";
}
