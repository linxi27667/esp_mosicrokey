#include "mosicrokey/hal_display.hpp"
#include "mosicrokey/board_config.hpp"
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <algorithm>
#include "sdkconfig.h"

namespace mosicrokey::hal
{
namespace
{
esp_lcd_panel_io_handle_t g_panel = nullptr;
SemaphoreHandle_t g_transfer_done = nullptr;
bool g_bus_ready = false;
bool g_display_ready = false;

struct display_init_command_t
{
    uint8_t command;
    uint8_t size;
    uint8_t bytes[16];
    unsigned delay_ms;
};

// 控制器格式不能混用；ST7789 参数依据固定 ESP-IDF v6.1 的原生驱动。
#if defined(CONFIG_MOSI_LCD_ST7789)
constexpr const char* g_controller_name = "ST7789";
constexpr display_init_command_t g_init_commands[] = {
    {0x01, 0, {}, 150},
    {0x11, 0, {}, 120},
    {0x3a, 1, {0x55}, 0},      // RGB565：DPI/DBI 均为 16 bit。
    {0xb0, 2, {0x00, 0xf0}, 0} // RAMCTRL：大端，与 Flush 的字节交换一致。
};
#elif defined(CONFIG_MOSI_LCD_ST7735)
constexpr const char* g_controller_name = "ST7735R";
constexpr display_init_command_t g_init_commands[] = {
    {0x01, 0, {}, 150},
    {0x11, 0, {}, 120},
    {0xb1, 3, {0x01, 0x2c, 0x2d}, 0},
    {0xb2, 3, {0x01, 0x2c, 0x2d}, 0},
    {0xb3, 6, {0x01, 0x2c, 0x2d, 0x01, 0x2c, 0x2d}, 0},
    {0xb4, 1, {0x07}, 0},
    {0xc0, 3, {0xa2, 0x02, 0x84}, 0},
    {0xc1, 1, {0xc5}, 0},
    {0xc2, 2, {0x0a, 0x00}, 0},
    {0xc3, 2, {0x8a, 0x2a}, 0},
    {0xc4, 2, {0x8a, 0xee}, 0},
    {0xc5, 1, {0x0e}, 0},
    {0x3a, 1, {0x05}, 0},
    {0xe0,
     16,
     {0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2b, 0x39, 0x00, 0x01, 0x03, 0x10},
     0},
    {0xe1,
     16,
     {0x03, 0x1d, 0x07, 0x06, 0x2e, 0x2c, 0x29, 0x2d, 0x2e, 0x2e, 0x37, 0x3f, 0x00, 0x00, 0x02, 0x10},
     0}};
#else
#error "Select the TFT controller in MosiCrokey hardware Kconfig"
#endif

bool Display_Transfer_Done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*)
{
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(g_transfer_done, &wake);
    return wake == pdTRUE;
}

esp_err_t Display_Send_Command(uint8_t command, const void* bytes = nullptr, size_t size = 0)
{
    return esp_lcd_panel_io_tx_param(g_panel, command, bytes, size);
}

void Display_Release_Resources()
{
    if (g_panel)
    {
        esp_lcd_panel_io_del(g_panel);
        g_panel = nullptr;
    }
    if (g_transfer_done)
    {
        vSemaphoreDelete(g_transfer_done);
        g_transfer_done = nullptr;
    }
    if (g_bus_ready)
    {
        spi_bus_free(SPI2_HOST);
        g_bus_ready = false;
    }
    g_display_ready = false;
}

esp_err_t Display_Configure_Controller()
{
    auto reset = static_cast<gpio_num_t>(CONFIG_MOSI_LCD_RST);
    esp_err_t error = gpio_set_direction(reset, GPIO_MODE_OUTPUT);
    if (error != ESP_OK)
    {
        return error;
    }
    error = gpio_set_level(reset, 0);
    if (error != ESP_OK)
    {
        return error;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    error = gpio_set_level(reset, 1);
    if (error != ESP_OK)
    {
        return error;
    }
    vTaskDelay(pdMS_TO_TICKS(120));
    for (const auto& item : g_init_commands)
    {
        error = Display_Send_Command(item.command, item.bytes, item.size);
        if (error != ESP_OK)
        {
            return error;
        }
        if (item.delay_ms)
        {
            vTaskDelay(pdMS_TO_TICKS(item.delay_ms));
        }
    }
    uint8_t orientation = 0x60; // MV | MX：160×128 横屏。
#ifdef CONFIG_MOSI_LCD_BGR
    orientation |= 0x08;
#endif
#ifdef CONFIG_MOSI_LCD_ROTATE_180
    orientation ^= 0xc0;
#endif
    error = Display_Send_Command(0x36, &orientation, 1);
    if (error != ESP_OK)
    {
        return error;
    }
#ifdef CONFIG_MOSI_LCD_INVERT
    error = Display_Send_Command(0x21);
#else
    error = Display_Send_Command(0x20);
#endif
    if (error != ESP_OK)
    {
        return error;
    }
    // 格式、方向和反色均设定后再开启显示。
    error = Display_Send_Command(0x13);
    if (error != ESP_OK)
    {
        return error;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    error = Display_Send_Command(0x29);
    if (error != ESP_OK)
    {
        return error;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if (CONFIG_MOSI_LCD_BL >= 0)
    {
        auto backlight = static_cast<gpio_num_t>(CONFIG_MOSI_LCD_BL);
        error = gpio_set_direction(backlight, GPIO_MODE_OUTPUT);
        if (error == ESP_OK)
        {
#ifdef CONFIG_MOSI_LCD_BL_ACTIVE_LOW
            error = gpio_set_level(backlight, 0);
#else
            error = gpio_set_level(backlight, 1);
#endif
        }
    }
    return error;
}
} // namespace

//=====显示初始化=====
esp_err_t Hal_Display_Init()
{
    if (g_display_ready)
    {
        return ESP_OK;
    }
    spi_bus_config_t bus{};
    bus.mosi_io_num = CONFIG_MOSI_LCD_MOSI;
    bus.miso_io_num = -1;
    bus.sclk_io_num = CONFIG_MOSI_LCD_CLK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = board::config::display_transfer_bytes;
    esp_err_t error = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (error != ESP_OK)
    {
        return error;
    }
    g_bus_ready = true;
    g_transfer_done = xSemaphoreCreateBinary();
    if (!g_transfer_done)
    {
        Display_Release_Resources();
        return ESP_ERR_NO_MEM;
    }
    esp_lcd_panel_io_spi_config_t io{};
    io.cs_gpio_num = static_cast<gpio_num_t>(CONFIG_MOSI_LCD_CS);
    io.dc_gpio_num = static_cast<gpio_num_t>(CONFIG_MOSI_LCD_DC);
    io.spi_mode = 0;
    io.pclk_hz = CONFIG_MOSI_LCD_SPI_HZ;
    io.trans_queue_depth = 1;
    io.lcd_cmd_bits = 8;
    io.lcd_param_bits = 8;
    io.on_color_trans_done = Display_Transfer_Done;
    error = esp_lcd_new_panel_io_spi(SPI2_HOST, &io, &g_panel);
    if (error == ESP_OK)
    {
        error = Display_Configure_Controller();
    }
    if (error != ESP_OK)
    {
        ESP_LOGE("mosi_lcd", "Controller setup failed: %s", esp_err_to_name(error));
        Display_Release_Resources();
        return error;
    }
    g_display_ready = true;
    ESP_LOGI("mosi_lcd", "%s 160x128 SPI=%dHz CLK=%d MOSI=%d CS=%d DC=%d RST=%d BL=%d offset=%d,%d",
             g_controller_name, CONFIG_MOSI_LCD_SPI_HZ, CONFIG_MOSI_LCD_CLK, CONFIG_MOSI_LCD_MOSI,
             CONFIG_MOSI_LCD_CS, CONFIG_MOSI_LCD_DC, CONFIG_MOSI_LCD_RST, CONFIG_MOSI_LCD_BL,
             CONFIG_MOSI_LCD_X_OFFSET, CONFIG_MOSI_LCD_Y_OFFSET);
    // 只有写接口，没有读取连接；此日志不能证明实体屏幕已经响应。
    ESP_LOGI("mosi_lcd", "SPI initialization complete; panel response is not probed");
    return ESP_OK;
}

//=====显示 DMA 提交=====
esp_err_t Hal_Display_Flush(int x1, int y1, int x2, int y2, uint16_t* pixels)
{
    if (!g_display_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (!pixels || x1 < 0 || y1 < 0 || x2 < x1 || y2 < y1 || x2 >= board::config::display_width ||
        y2 >= board::config::display_height)
    {
        return ESP_ERR_INVALID_ARG;
    }
    size_t size = static_cast<size_t>(x2 - x1 + 1) * (y2 - y1 + 1) * 2;
    if (size > board::config::display_transfer_bytes)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    x1 += CONFIG_MOSI_LCD_X_OFFSET;
    x2 += CONFIG_MOSI_LCD_X_OFFSET;
    y1 += CONFIG_MOSI_LCD_Y_OFFSET;
    y2 += CONFIG_MOSI_LCD_Y_OFFSET;
    uint8_t columns[] = {static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1),
                         static_cast<uint8_t>(x2 >> 8), static_cast<uint8_t>(x2)};
    uint8_t rows[] = {static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1), static_cast<uint8_t>(y2 >> 8),
                      static_cast<uint8_t>(y2)};
    esp_err_t error = Display_Send_Command(0x2a, columns, sizeof(columns));
    if (error == ESP_OK)
    {
        error = Display_Send_Command(0x2b, rows, sizeof(rows));
    }
    if (error != ESP_OK)
    {
        return error;
    }
    auto* bytes = reinterpret_cast<uint8_t*>(pixels);
    for (size_t i = 0; i < size; i += 2)
    {
        std::swap(bytes[i], bytes[i + 1]);
    }
    while (xSemaphoreTake(g_transfer_done, 0))
    {
    }
    error = esp_lcd_panel_io_tx_color(g_panel, 0x2c, pixels, size);
    if (error == ESP_OK && !xSemaphoreTake(g_transfer_done, pdMS_TO_TICKS(1000)))
    {
        // tx_param 会等待已提交颜色事务结束，不能提前复用/恢复 DMA 缓冲。
        // 排空失败时无法确认 DMA 已停止。立即终止而不是让 LVGL 复用缓冲。
        ESP_ERROR_CHECK(Display_Send_Command(0, nullptr, 0));
        error = ESP_ERR_TIMEOUT;
    }
    for (size_t i = 0; i < size; i += 2)
    {
        std::swap(bytes[i], bytes[i + 1]);
    }
    return error;
}
} // namespace mosicrokey::hal
