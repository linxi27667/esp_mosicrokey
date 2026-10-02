#pragma once
#include "driver/gpio.h"
#include <cstddef>
struct MockPanel;
using esp_lcd_panel_io_handle_t = MockPanel*;
struct esp_lcd_panel_io_event_data_t {};
struct esp_lcd_panel_io_spi_config_t
{
    gpio_num_t cs_gpio_num = -1, dc_gpio_num = -1;
    int spi_mode = 0, pclk_hz = 0, trans_queue_depth = 0;
    int lcd_cmd_bits = 0, lcd_param_bits = 0;
    bool (*on_color_trans_done)(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) = nullptr;
};
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t, int, const void*, size_t);
esp_err_t esp_lcd_panel_io_tx_color(esp_lcd_panel_io_handle_t, int, const void*, size_t);
esp_err_t esp_lcd_new_panel_io_spi(int, const esp_lcd_panel_io_spi_config_t*, esp_lcd_panel_io_handle_t*);
esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t);
