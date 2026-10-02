#pragma once
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_err.h"
struct esp_vfs_fat_mount_config_t
{
    bool format_if_mount_failed = false;
    int max_files = 0;
    unsigned allocation_unit_size = 0;
};
using esp_vfs_fat_sdmmc_mount_config_t = esp_vfs_fat_mount_config_t;
esp_err_t esp_vfs_fat_sdcard_unmount(const char*, sdmmc_card_t*);
esp_err_t esp_vfs_fat_sdmmc_mount(const char*, const sdmmc_host_t*, const sdmmc_slot_config_t*,
                                const esp_vfs_fat_sdmmc_mount_config_t*, sdmmc_card_t**);
esp_err_t esp_vfs_fat_spiflash_mount_ro(const char*, const char*, const esp_vfs_fat_mount_config_t*);
