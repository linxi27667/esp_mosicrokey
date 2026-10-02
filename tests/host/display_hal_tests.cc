#include "mosicrokey/hal_display.hpp"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "freertos/semphr.h"
#include <iostream>
#include <cstdlib>

struct MockPanel {};
using namespace mosicrokey;
namespace {
unsigned checks = 0, bus_frees = 0, panel_deletes = 0, semaphore_deletes = 0, colors = 0;
MockPanel panel;
unsigned semaphore_count = 0;
bool fail_semaphore = false, signal_completion = true, dma_active = false;
esp_err_t bus_error = ESP_FAIL, panel_error = ESP_OK, gpio_error = ESP_OK;
esp_err_t param_error = ESP_OK, drain_error = ESP_OK, color_error = ESP_OK;
esp_lcd_panel_io_spi_config_t io;
uint8_t orientation = 0;
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
esp_err_t spi_bus_initialize(int host, const spi_bus_config_t* config, int)
{
    Check(host == SPI2_HOST && config->mosi_io_num == 44 && config->sclk_io_num == 43 &&
          config->max_transfer_sz == 6400, "TFT uses configured S31 SPI and bounded DMA buffer");
    return bus_error;
}
esp_err_t spi_bus_free(int)
{
    ++bus_frees;
    return ESP_OK;
}
esp_err_t gpio_set_direction(gpio_num_t, int)
{
    return gpio_error;
}
esp_err_t gpio_set_level(gpio_num_t, int)
{
    return gpio_error;
}
SemaphoreHandle_t xSemaphoreCreateBinary()
{
    semaphore_count = 0;
    return fail_semaphore ? nullptr : &semaphore_count;
}
void vSemaphoreDelete(SemaphoreHandle_t)
{
    ++semaphore_deletes;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t)
{
    if (!semaphore_count) return pdFALSE;
    --semaphore_count;
    return pdTRUE;
}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t, BaseType_t* wake)
{
    semaphore_count = 1;
    *wake = pdFALSE;
    return pdTRUE;
}
void vTaskDelay(TickType_t) {}
esp_err_t esp_lcd_new_panel_io_spi(int, const esp_lcd_panel_io_spi_config_t* config,
                                 esp_lcd_panel_io_handle_t* output)
{
    Check(config->cs_gpio_num == 45 && config->dc_gpio_num == 46 && config->pclk_hz == 20000000 &&
          config->trans_queue_depth == 1, "TFT 20MHz typed pins and single transaction owner");
    if (panel_error == ESP_OK)
    {
        io = *config;
        *output = &panel;
    }
    return panel_error;
}
esp_err_t esp_lcd_panel_io_del(esp_lcd_panel_io_handle_t)
{
    ++panel_deletes;
    return ESP_OK;
}
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t, int command, const void* data, size_t size)
{
    if (command == 0x36 && size == 1 && param_error == ESP_OK)
    {
        orientation = *static_cast<const uint8_t*>(data);
    }
    if (command == 0)
    {
        if (drain_error == ESP_OK) dma_active = false;
        return drain_error;
    }
    return param_error;
}
esp_err_t esp_lcd_panel_io_tx_color(esp_lcd_panel_io_handle_t handle, int command,
                                  const void* pixels, size_t size)
{
    ++colors;
    auto* bytes = static_cast<const uint8_t*>(pixels);
    Check(command == 0x2c && size == 4 && bytes[0] == 0x12 && bytes[1] == 0x34,
          "only swapped RGB565 bytes submitted to DMA");
    if (color_error != ESP_OK) return color_error;
    dma_active = true;
    if (signal_completion)
    {
        dma_active = false;
        io.on_color_trans_done(handle, nullptr, nullptr);
    }
    return ESP_OK;
}
int main()
{
    uint16_t pixels[] = {0x1234, 0xabcd};
    Check(hal::Hal_Display_Flush(0,0,1,0,pixels) == ESP_ERR_INVALID_STATE, "uninitialized display rejected");
    Check(hal::Hal_Display_Init() == ESP_FAIL && !bus_frees, "bus init error retains no resources");
    bus_error = ESP_OK; fail_semaphore = true;
    Check(hal::Hal_Display_Init() == ESP_ERR_NO_MEM && bus_frees == 1, "semaphore failure frees SPI bus");
    fail_semaphore = false; panel_error = ESP_FAIL;
    Check(hal::Hal_Display_Init() == ESP_FAIL && bus_frees == 2 && semaphore_deletes == 1,
          "panel creation failure releases semaphore and bus");
    panel_error = ESP_OK; gpio_error = ESP_FAIL;
    Check(hal::Hal_Display_Init() == ESP_FAIL && panel_deletes == 1 && bus_frees == 3,
          "reset failure releases initialized panel");
    gpio_error = ESP_OK; param_error = ESP_FAIL;
    Check(hal::Hal_Display_Init() == ESP_FAIL && panel_deletes == 2 && bus_frees == 4,
          "controller init failure releases all resources");
    param_error = ESP_OK;
    Check(hal::Hal_Display_Init() == ESP_OK && hal::Hal_Display_Init() == ESP_OK,
          "display retries and initialization is idempotent");
    Check(orientation == 0x68,"default remains ST7735 landscape BGR");
    Check(hal::Hal_Display_Flush(0,0,1,0,nullptr) == ESP_ERR_INVALID_ARG, "null pixels rejected");
    Check(hal::Hal_Display_Flush(-1,0,1,0,pixels) == ESP_ERR_INVALID_ARG, "negative bounds rejected");
    Check(hal::Hal_Display_Flush(0,0,160,0,pixels) == ESP_ERR_INVALID_ARG, "horizontal overflow rejected");
    Check(hal::Hal_Display_Flush(0,0,1,128,pixels) == ESP_ERR_INVALID_ARG, "vertical overflow rejected");
    Check(hal::Hal_Display_Flush(1,0,0,0,pixels) == ESP_ERR_INVALID_ARG, "reversed bounds rejected");
    Check(hal::Hal_Display_Flush(0,0,159,20,pixels) == ESP_ERR_INVALID_SIZE && !colors,
          "oversized DMA region rejected before reading buffer");
    Check(hal::Hal_Display_Flush(0,0,1,0,pixels) == ESP_OK && pixels[0] == 0x1234 && pixels[1] == 0xabcd,
          "normal completion restores caller buffer");
    param_error = ESP_FAIL;
    Check(hal::Hal_Display_Flush(0,0,1,0,pixels) == ESP_FAIL && pixels[0] == 0x1234,
          "window command error leaves pixels untouched");
    param_error = ESP_OK; color_error = ESP_FAIL;
    Check(hal::Hal_Display_Flush(0,0,1,0,pixels) == ESP_FAIL && pixels[0] == 0x1234,
          "rejected color submission restores pixels");
    color_error = ESP_OK; signal_completion = false;
    Check(hal::Hal_Display_Flush(0,0,1,0,pixels) == ESP_ERR_TIMEOUT && !dma_active && pixels[0] == 0x1234,
          "timeout drains DMA before restoring pixels");
    drain_error = ESP_FAIL;
    bool stopped = false;
    try {hal::Hal_Display_Flush(0,0,1,0,pixels);}
    catch (const MockEspError& failure) {stopped = failure.error == ESP_FAIL;}
    auto* bytes = reinterpret_cast<uint8_t*>(pixels);
    Check(stopped && dma_active && bytes[0] == 0x12 && bytes[1] == 0x34,
          "drain failure stops without returning or restoring DMA-owned buffer");
    std::cout << "PASS " << checks << " checks: actual HAL display cleanup, bounds and DMA ownership\n";
}
