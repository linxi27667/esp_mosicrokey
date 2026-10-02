#pragma once
#include "driver/gpio.h"
struct sdmmc_host_t {int slot = 0, max_freq_khz = 0;};
struct sdmmc_slot_config_t
{
    int width = 0;
    gpio_num_t clk = -1, cmd = -1, d0 = -1, d1 = -1, d2 = -1, d3 = -1;
    gpio_num_t gpio_cd = -1, gpio_wp = -1;
};
#define SDMMC_HOST_DEFAULT() sdmmc_host_t{}
#define SDMMC_SLOT_CONFIG_DEFAULT() sdmmc_slot_config_t{}
