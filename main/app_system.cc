#include "app_system.hpp"
#include "mosicrokey/app.hpp"
#include "mosicrokey/console.hpp"
#include "mosicrokey/hal_storage.hpp"
#include "mosicrokey/board_config.hpp"
#include "mosicrokey/game.hpp"
#include "mosicrokey/ui.hpp"
#include "mosicrokey/xiaozhi.hpp"
#include "mosicrokey/espclaw.hpp"
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_app_desc.h>
#include <esp_system.h>
#include <cstdlib>
#include <ctime>

namespace mosicrokey
{
namespace
{
App g_app;

void Startup_Report(const Result& result)
{
    if (!result)
    {
        ESP_LOGW("MosiCrokey", "%s", result.message.c_str());
        g_app.Notice(result.message);
    }
}
} // namespace

Result App_System_Init()
{
    // 不自动擦除 NVS，保留网络/模型配置。
    ESP_ERROR_CHECK(nvs_flash_init());
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI("MosiCrokey", "v%s S31: xiaozhi ed5cd7ba / ESPClaw 74b18700 reset_reason=%d",
             esp_app_get_description()->version, static_cast<int>(esp_reset_reason()));
    auto error = hal::Hal_Storage_Mount_System();
    if (error != ESP_OK)
    {
        ESP_LOGW("MosiCrokey", "System skills mount: %s", esp_err_to_name(error));
    }
    error = hal::Hal_Storage_Mount();
    if (error != ESP_OK)
    {
        Startup_Report(Result::Fail(Error::Storage, std::string("TF: ") + esp_err_to_name(error)));
    }
    Startup_Report(ui::Start());
    static GameService games(board::config::card_path, g_app);
    static auto voice = CreateXiaozhi(g_app);
    // 先初始化唯一 Wi-Fi 所有者，再启动 APP 的 esp_netif 状态轮询。
    InitializeXiaozhi();
    static auto agent = CreateAgent(games, g_app);
    auto result = g_app.System_Start(*voice, *agent, games, games);
    if (!result)
    {
        return result;
    }
    Startup_Report(Key_Scan_Task_Create(g_app));
    Startup_Report(Console_Task_Create(g_app));
    return {};
}

void App_System_Run()
{
    auto result = App_System_Init();
    if (!result)
    {
        Startup_Report(result);
        return;
    }
    RunXiaozhi();
}
} // namespace mosicrokey
