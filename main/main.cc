#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_vfs_fat.h>
#include <cstdlib>
#include <ctime>
#include <esp_app_desc.h>
#include <esp_system.h>
#include "mosicrokey/app.hpp"
#include "mosicrokey/board.hpp"
#include "mosicrokey/game.hpp"
#include "mosicrokey/ui.hpp"
#include "mosicrokey/xiaozhi.hpp"
#include "mosicrokey/espclaw.hpp"
extern "C" void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());
    setenv("TZ","CST-8",1); tzset();
    ESP_LOGI("MosiCrokey", "v%s S31: xiaozhi ed5cd7ba / ESPClaw 74b18700 reset_reason=%d",esp_app_get_description()->version,static_cast<int>(esp_reset_reason()));
    using namespace mosicrokey;
    static App app;
    auto report=[&](const Result& result){if(!result) {ESP_LOGW("MosiCrokey","%s",result.message.c_str()); app.Notice(result.message);}};
    esp_vfs_fat_mount_config_t mount{}; mount.max_files=4; mount.format_if_mount_failed=false;
    auto error=esp_vfs_fat_spiflash_mount_ro("/system","system",&mount);
    if(error!=ESP_OK) ESP_LOGW("MosiCrokey","System skills mount: %s",esp_err_to_name(error));
    report(board::MountCard());
    report(ui::Start());
    static GameService games("/sdcard",app);
    static auto voice=CreateXiaozhi(app);
    InitializeXiaozhi(); // Initialize the sole WiFi owner before App polls esp_netif.
    static auto agent=CreateAgent(games,app);
    auto result=app.Start(*voice,*agent,games,games);
    if(!result) {report(result); return;}
    report(board::StartKeys([&](Key key,bool pressed){app.Input(key,pressed);}));
    report(StartConsole(app));
    RunXiaozhi(); // The upstream application loop runs on the main task at its original priority.
}
