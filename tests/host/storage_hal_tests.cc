#include "mosicrokey/hal_storage.hpp"
#include "esp_vfs_fat.h"
#include <cstring>
#include <iostream>
#include <cstdlib>

using namespace mosicrokey;
namespace {
unsigned checks = 0, mount_calls = 0, unmount_calls = 0, system_calls = 0;
sdmmc_card_t card{};
bool context_live = false;
esp_err_t mount_error = ESP_FAIL, unmount_error = ESP_OK, system_error = ESP_FAIL;
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
esp_err_t esp_vfs_fat_sdcard_unmount(const char* path, sdmmc_card_t* handle)
{
    ++unmount_calls;
    Check(context_live && handle == &card && std::strcmp(path, "/sdcard") == 0,
          "unmount never receives a released SDK context");
    // 对应本机 IDF：先释放 context/card，再返回路径注销结果。
    context_live = false;
    return unmount_error;
}
esp_err_t esp_vfs_fat_sdmmc_mount(const char* path, const sdmmc_host_t* host,
                                const sdmmc_slot_config_t* slot,
                                const esp_vfs_fat_sdmmc_mount_config_t* config, sdmmc_card_t** output)
{
    ++mount_calls;
    Check(std::strcmp(path, "/sdcard") == 0 && host->slot == 0 && host->max_freq_khz == 20000 &&
          slot->width == 4 && slot->d0 == 20 && slot->d1 == 21 && slot->d2 == 22 &&
          slot->d3 == 23 && slot->clk == 24 && slot->cmd == 25, "S31 four-wire 20MHz SDMMC pins");
    Check(!config->format_if_mount_failed && config->max_files == 12,
          "failure never enables formatting");
    if (mount_error == ESP_OK)
    {
        context_live = true;
        *output = &card;
    }
    return mount_error;
}
esp_err_t esp_vfs_fat_spiflash_mount_ro(const char* path, const char* partition,
                                     const esp_vfs_fat_mount_config_t* config)
{
    ++system_calls;
    Check(std::strcmp(path, "/system") == 0 && std::strcmp(partition, "system") == 0 &&
          !config->format_if_mount_failed && config->max_files == 4, "system skills mounted read-only");
    return system_error;
}
int main()
{
    Check(!hal::Hal_Storage_Is_Mounted(), "starts unavailable");
    Check(hal::Hal_Storage_Mount() == ESP_FAIL && !hal::Hal_Storage_Is_Mounted(), "missing card propagated");
    mount_error = ESP_OK;
    Check(hal::Hal_Storage_Mount() == ESP_OK && hal::Hal_Storage_Is_Mounted(), "mount retry succeeds");
    hal::Hal_Storage_Mark_Failed();
    Check(!hal::Hal_Storage_Is_Mounted() && context_live && !unmount_calls, "mark failed does not unmount live files");
    Check(hal::Hal_Storage_Mount() == ESP_OK && unmount_calls == 1 && hal::Hal_Storage_Is_Mounted(),
          "explicit remount releases previous SDK card");
    unmount_error = ESP_FAIL;
    Check(hal::Hal_Storage_Mount() == ESP_FAIL && !context_live && !hal::Hal_Storage_Is_Mounted(),
          "post-release unmount error is reported");
    unsigned before = unmount_calls;
    Check(hal::Hal_Storage_Mount() == ESP_OK && unmount_calls == before,
          "retry mounts without reusing freed card handle");
    Check(hal::Hal_Storage_Mount_System() == ESP_FAIL, "read-only mount failure propagated");
    system_error = ESP_OK;
    Check(hal::Hal_Storage_Mount_System() == ESP_OK, "read-only mount retries");
    before = system_calls;
    Check(hal::Hal_Storage_Mount_System() == ESP_OK && before == system_calls, "system mount idempotent");
    std::cout << "PASS " << checks << " checks: actual HAL storage ownership and partial-unmount failure\n";
}
