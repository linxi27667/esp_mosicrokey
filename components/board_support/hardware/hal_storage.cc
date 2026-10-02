#include "mosicrokey/hal_storage.hpp"
#include "mosicrokey/board_config.hpp"
#include <esp_vfs_fat.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
#include <atomic>

namespace mosicrokey::hal
{
namespace
{
sdmmc_card_t* g_card = nullptr;
std::atomic<bool> g_mounted{false};
bool g_system_mounted = false;
} // namespace

//=====TF 挂载与明确重挂=====
esp_err_t Hal_Storage_Mount()
{
    g_mounted = false;
    if (g_card)
    {
        // SDK 会在返回路径注销错误前释放 card/context，不能保留可能失效的句柄。
        auto* card = g_card;
        g_card = nullptr;
        esp_err_t error = esp_vfs_fat_sdcard_unmount(board::config::card_path, card);
        if (error != ESP_OK)
        {
            return error;
        }
    }
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = 0;
    host.max_freq_khz = board::config::sd_khz;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = static_cast<gpio_num_t>(board::config::sd_clk);
    slot.cmd = static_cast<gpio_num_t>(board::config::sd_cmd);
    slot.d0 = static_cast<gpio_num_t>(board::config::sd_d0);
    slot.d1 = static_cast<gpio_num_t>(board::config::sd_d1);
    slot.d2 = static_cast<gpio_num_t>(board::config::sd_d2);
    slot.d3 = static_cast<gpio_num_t>(board::config::sd_d3);
    slot.gpio_cd = GPIO_NUM_NC;
    slot.gpio_wp = GPIO_NUM_NC;
    esp_vfs_fat_sdmmc_mount_config_t config{};
    config.format_if_mount_failed = false;
    config.max_files = 12;
    config.allocation_unit_size = 16384;
    esp_err_t error = esp_vfs_fat_sdmmc_mount(board::config::card_path, &host, &slot, &config, &g_card);
    g_mounted = error == ESP_OK;
    return error;
}

bool Hal_Storage_Is_Mounted()
{
    return g_mounted.load();
}

void Hal_Storage_Mark_Failed()
{
    g_mounted = false;
}

esp_err_t Hal_Storage_Mount_System()
{
    if (g_system_mounted)
    {
        return ESP_OK;
    }
    esp_vfs_fat_mount_config_t config{};
    config.max_files = 4;
    config.format_if_mount_failed = false;
    esp_err_t error = esp_vfs_fat_spiflash_mount_ro("/system", "system", &config);
    g_system_mounted = error == ESP_OK;
    return error;
}
} // namespace mosicrokey::hal
