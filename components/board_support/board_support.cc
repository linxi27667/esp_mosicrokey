#include "mosicrokey/board.hpp"
#include "mosicrokey/board_config.hpp"
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_vfs_fat.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include "sdkconfig.h"

namespace mosicrokey::board {
static_assert(pdMS_TO_TICKS(5)>0,"MosiCrokey requires FreeRTOS ticks fine enough for 5 ms scanning");
namespace {
esp_lcd_panel_io_handle_t panel = nullptr;
SemaphoreHandle_t transfer_done = nullptr;
sdmmc_card_t* card = nullptr;
std::atomic<bool> mounted{false};
std::function<void(Key, bool)> key_callback;
bool TransferDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
    BaseType_t wake = pdFALSE; xSemaphoreGiveFromISR(transfer_done, &wake); return wake == pdTRUE;
}
Result Command(uint8_t command, const void* bytes = nullptr, size_t size = 0) {
    auto error = esp_lcd_panel_io_tx_param(panel, command, bytes, size);
    return error == ESP_OK ? Result{} : Result::Fail(Error::Runtime, esp_err_to_name(error));
}
void KeyTask(void*) {
    bool stable[6] = {}, previous[6] = {}; unsigned count[6] = {};
    for (;;) {
        for (unsigned i = 0; i < 6; ++i) {
            bool pressed = gpio_get_level(static_cast<gpio_num_t>(config::keys[i])) == 0;
            count[i] = pressed == previous[i] ? std::min(count[i] + 1, 4u) : 0;
            previous[i] = pressed;
            if (count[i] == 4 && stable[i] != pressed) { stable[i] = pressed; key_callback(static_cast<Key>(i), pressed); }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
}
Result InitializeDisplay() {
    spi_bus_config_t bus{};
    bus.mosi_io_num = CONFIG_MOSI_LCD_MOSI; bus.miso_io_num = -1; bus.sclk_io_num = CONFIG_MOSI_LCD_CLK;
    bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = kWidth * 20 * 2;
    auto error = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (error != ESP_OK) return Result::Fail(Error::Runtime, esp_err_to_name(error));
    transfer_done = xSemaphoreCreateBinary();
    if (!transfer_done) { spi_bus_free(SPI2_HOST); return Result::Fail(Error::Runtime, "Display semaphore allocation failed"); }
    esp_lcd_panel_io_spi_config_t config{};
    config.cs_gpio_num = static_cast<gpio_num_t>(CONFIG_MOSI_LCD_CS); config.dc_gpio_num = static_cast<gpio_num_t>(CONFIG_MOSI_LCD_DC);
    config.spi_mode = 0; config.pclk_hz = mosicrokey::board::config::spi_hz; config.trans_queue_depth = 1;
    config.lcd_cmd_bits = 8; config.lcd_param_bits = 8; config.on_color_trans_done = TransferDone;
    error = esp_lcd_new_panel_io_spi(SPI2_HOST, &config, &panel);
    if (error != ESP_OK) { vSemaphoreDelete(transfer_done); transfer_done = nullptr; spi_bus_free(SPI2_HOST); return Result::Fail(Error::Runtime, esp_err_to_name(error)); }
    gpio_set_direction(static_cast<gpio_num_t>(CONFIG_MOSI_LCD_RST), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_MOSI_LCD_RST), 0); vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_MOSI_LCD_RST), 1); vTaskDelay(pdMS_TO_TICKS(120));
    struct Init { uint8_t command, size, bytes[16]; unsigned delay; };
    static constexpr Init sequence[] = {
        {0x01,0,{},150}, {0x11,0,{},120},
        {0xb1,3,{0x01,0x2c,0x2d},0}, {0xb2,3,{0x01,0x2c,0x2d},0},
        {0xb3,6,{0x01,0x2c,0x2d,0x01,0x2c,0x2d},0}, {0xb4,1,{0x07},0},
        {0xc0,3,{0xa2,0x02,0x84},0}, {0xc1,1,{0xc5},0}, {0xc2,2,{0x0a,0x00},0},
        {0xc3,2,{0x8a,0x2a},0}, {0xc4,2,{0x8a,0xee},0}, {0xc5,1,{0x0e},0},
        {0x36,1,{0x68},0}, {0x3a,1,{0x05},0},
        {0xe0,16,{0x02,0x1c,0x07,0x12,0x37,0x32,0x29,0x2d,0x29,0x25,0x2b,0x39,0x00,0x01,0x03,0x10},0},
        {0xe1,16,{0x03,0x1d,0x07,0x06,0x2e,0x2c,0x29,0x2d,0x2e,0x2e,0x37,0x3f,0x00,0x00,0x02,0x10},0},
        {0x13,0,{},10}, {0x29,0,{},100}};
    for (const auto& item : sequence) {
        auto result = Command(item.command, item.bytes, item.size); if (!result) return result;
        if (item.delay) vTaskDelay(pdMS_TO_TICKS(item.delay));
    }
#ifdef CONFIG_MOSI_LCD_INVERT
    auto result = Command(0x21);
#else
    auto result = Command(0x20);
#endif
    if (!result) return result;
    if (CONFIG_MOSI_LCD_BL >= 0) {
        gpio_set_direction(static_cast<gpio_num_t>(CONFIG_MOSI_LCD_BL), GPIO_MODE_OUTPUT);
        gpio_set_level(static_cast<gpio_num_t>(CONFIG_MOSI_LCD_BL), 1);
    }
    return {};
}
void Flush(int x1, int y1, int x2, int y2, void* pixels) {
    if (!panel) return;
    x1 += CONFIG_MOSI_LCD_X_OFFSET; x2 += CONFIG_MOSI_LCD_X_OFFSET;
    y1 += CONFIG_MOSI_LCD_Y_OFFSET; y2 += CONFIG_MOSI_LCD_Y_OFFSET;
    uint8_t columns[] = {static_cast<uint8_t>(x1 >> 8),static_cast<uint8_t>(x1),static_cast<uint8_t>(x2 >> 8),static_cast<uint8_t>(x2)};
    uint8_t rows[] = {static_cast<uint8_t>(y1 >> 8),static_cast<uint8_t>(y1),static_cast<uint8_t>(y2 >> 8),static_cast<uint8_t>(y2)};
    if (!Command(0x2a, columns, 4) || !Command(0x2b, rows, 4)) return;
    size_t size = (x2-x1+1)*(y2-y1+1)*2;
    auto* bytes = static_cast<uint8_t*>(pixels);
    for (size_t i = 0; i < size; i += 2) std::swap(bytes[i], bytes[i+1]);
    while (xSemaphoreTake(transfer_done, 0)) {}
    auto error = esp_lcd_panel_io_tx_color(panel, 0x2c, pixels, size);
    if (error == ESP_OK && !xSemaphoreTake(transfer_done, pdMS_TO_TICKS(1000))) {
        // Keep the DMA buffer alive until the driver drains its transaction.
        esp_lcd_panel_io_tx_param(panel, 0, nullptr, 0);
    }
    for (size_t i = 0; i < size; i += 2) std::swap(bytes[i], bytes[i+1]);
}
Result MountCard() {
    if (card) { auto error = esp_vfs_fat_sdcard_unmount("/sdcard", card); if (error != ESP_OK) return Result::Fail(Error::Storage, esp_err_to_name(error)); card = nullptr; }
    mounted = false;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT(); host.slot = 0; host.max_freq_khz = config::sd_khz;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT(); slot.width = 4;
    slot.clk = static_cast<gpio_num_t>(config::sd_clk); slot.cmd = static_cast<gpio_num_t>(config::sd_cmd);
    slot.d0 = static_cast<gpio_num_t>(config::sd_d0); slot.d1 = static_cast<gpio_num_t>(config::sd_d1);
    slot.d2 = static_cast<gpio_num_t>(config::sd_d2); slot.d3 = static_cast<gpio_num_t>(config::sd_d3);
    slot.gpio_cd = GPIO_NUM_NC; slot.gpio_wp = GPIO_NUM_NC;
    esp_vfs_fat_sdmmc_mount_config_t mount{};
    mount.format_if_mount_failed = false; mount.max_files = 12; mount.allocation_unit_size = 16384;
    auto error = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount, &card);
    mounted = error == ESP_OK;
    return mounted ? Result{} : Result::Fail(Error::Storage, std::string("TF: ") + esp_err_to_name(error));
}
bool CardMounted() { return mounted.load(); }
void SetStorageFailed() { mounted = false; }
Result StartKeys(std::function<void(Key, bool)> callback) {
    key_callback = std::move(callback);
    gpio_config_t config{}; config.mode = GPIO_MODE_INPUT; config.pull_up_en = GPIO_PULLUP_ENABLE;
    for (auto key : mosicrokey::board::config::keys) config.pin_bit_mask |= 1ull << key;
    auto error = gpio_config(&config); if (error != ESP_OK) return Result::Fail(Error::Runtime, esp_err_to_name(error));
    if (xTaskCreate(KeyTask, "mkeys", 4096, nullptr, 3, nullptr) != pdPASS) return Result::Fail(Error::Runtime, "Key task allocation failed");
    return {};
}
} // namespace mosicrokey::board
