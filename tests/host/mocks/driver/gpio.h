#pragma once
#include "esp_err.h"
#include <cstdint>
using gpio_num_t = int;
constexpr int GPIO_MODE_INPUT = 1, GPIO_PULLUP_ENABLE = 1;
constexpr gpio_num_t GPIO_NUM_NC = -1;
constexpr int GPIO_MODE_OUTPUT = 2;
constexpr int GPIO_INTR_ANYEDGE = 3;
struct gpio_config_t
{
    uint64_t pin_bit_mask = 0;
    int mode = 0, pull_up_en = 0, intr_type = 0;
};
esp_err_t gpio_config(const gpio_config_t* config);
int gpio_get_level(gpio_num_t pin);
esp_err_t gpio_set_direction(gpio_num_t, int);
esp_err_t gpio_set_level(gpio_num_t, int);
esp_err_t gpio_install_isr_service(int);
void gpio_uninstall_isr_service();
esp_err_t gpio_isr_handler_add(gpio_num_t, void (*)(void*), void*);
esp_err_t gpio_isr_handler_remove(gpio_num_t);
