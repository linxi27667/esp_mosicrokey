#include "mosicrokey/console.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_app_desc.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <mbedtls/base64.h>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <algorithm>

extern const char snake_text[] asm("_binary_main_lua_start");
namespace mosicrokey
{
namespace
{
void Report(const Result& result)
{
    std::printf("mosi: %s\n", result ? "OK" : result.message.c_str());
}
void CaptureFrame(App& app)
{
    std::vector<uint16_t> frame;
    auto result = app.Game_Capture_Frame(frame);
    if (!result)
    {
        Report(result);
        return;
    }
    uint32_t checksum = 2166136261u;
    for (uint16_t pixel : frame)
    {
        checksum = (checksum ^ static_cast<uint8_t>(pixel)) * 16777619u;
        checksum = (checksum ^ static_cast<uint8_t>(pixel >> 8)) * 16777619u;
    }
    std::printf("MOSI_FRAME_BEGIN %d %d %08lx\n", kWidth, kHeight, static_cast<unsigned long>(checksum));
    // Freeze one actual VM frame. Encode rows to allow recovery around background logs.
    unsigned char row[kWidth * 2], encoded[(kWidth * 2 + 2) / 3 * 4 + 1];
    for (int y = 0; y < kHeight; ++y)
    {
        for (int x = 0; x < kWidth; ++x)
        {
            auto pixel = frame[y * kWidth + x];
            row[x * 2] = pixel;
            row[x * 2 + 1] = pixel >> 8;
        }
        size_t length = 0;
        if (mbedtls_base64_encode(encoded, sizeof(encoded), &length, row, sizeof(row)) != 0)
        {
            Report(Result::Fail(Error::Runtime, "Frame encoding failed"));
            return;
        }
        std::printf("MOSI_FRAME_ROW %d %.*s\n", y, static_cast<int>(length), encoded);
    }
    std::puts("MOSI_FRAME_END");
}
void TaskStatus()
{
#if CONFIG_FREERTOS_USE_TRACE_FACILITY
    auto capacity = uxTaskGetNumberOfTasks() + 4;
    auto* tasks = static_cast<TaskStatus_t*>(
        heap_caps_calloc(capacity, sizeof(TaskStatus_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!tasks)
    {
        std::puts("task status: insufficient diagnostic memory");
        return;
    }
    auto count = uxTaskGetSystemState(tasks, capacity, nullptr);
    for (UBaseType_t i = 0; i < count; ++i)
    {
        std::printf("task=%s priority=%u state=%u stack_free_min_bytes=%u core_affinity=%ld\n",
                    tasks[i].pcTaskName, static_cast<unsigned>(tasks[i].uxCurrentPriority),
                    static_cast<unsigned>(tasks[i].eCurrentState),
                    static_cast<unsigned>(tasks[i].usStackHighWaterMark * sizeof(StackType_t)),
                    static_cast<long>(xTaskGetCoreID(tasks[i].xHandle)));
    }
    heap_caps_free(tasks);
#endif
}
} // namespace
//=====串口命令业务=====
void Console_Command_Process(App& app, const std::string& line)
{
    auto space = line.find(' ');
    auto command = line.substr(0, space);
    auto arguments = space == std::string::npos ? "" : line.substr(space + 1);
    if (command == "help")
    {
        std::puts("help | status | audio_status | game_selftest | frame | game_source <id> [offset] | page "
                  "home/voice/agent/games/settings/diag | ask <text> | gameask <create/modify; requires "
                  "publication> | cancel | wifi | wifi_config <JSON ssid,password; saves and restarts> | "
                  "remount | demo | key up/down/left/right/confirm/back [0/1] | agent_config <JSON "
                  "url,model,key> | reboot\nUART input is not echoed or stored in command history.");
    }
    else if (command == "ask")
    {
        Report(app.Agent_Ask(arguments));
    }
    else if (command == "gameask")
    {
        Report(app.Agent_Ask(arguments, AgentIntent::PublishGame));
    }
    else if (command == "cancel")
    {
        Report(app.Agent_Cancel());
    }
    else if (command == "wifi")
    {
        app.Wifi_Configure();
        Report({});
    }
    else if (command == "audio_status")
    {
        app.Audio_Diagnose();
        Report({});
    }
    else if (command == "frame")
    {
        CaptureFrame(app);
    }
    else if (command == "game_source")
    {
        std::istringstream input(arguments);
        std::string id, output;
        unsigned offset = 0;
        input >> id >> offset;
        auto result = app.Game_Read_Source(id, offset, output);
        if (result)
        {
            std::printf("MOSI_SOURCE_JSON %s\n", output.c_str());
        }
        else
        {
            Report(result);
        }
    }
    else if (command == "game_selftest")
    {
        std::string report;
        auto result = app.Game_Run_Self_Test(snake_text, report);
        Report(result);
        if (result)
        {
            std::printf("game_selftest: %s\n", report.c_str());
        }
    }
    else if (command == "remount")
    {
        Report(app.Storage_Remount());
    }
    else if (command == "demo")
    {
        Report(app.Game_Install_Example(snake_text));
    }
    else if (command == "reboot")
    {
        esp_restart();
    }
    else if (command == "status")
    {
        auto snapshot = app.State_Read();
        std::printf("mode=%d wifi=%d tf=%d request=%lu busy=%d games=%u heap=%lu "
                    "min=%lu\nvoice=%s\nnotice=%s\nagent=%s\n",
                    static_cast<int>(snapshot.mode), snapshot.wifi, snapshot.storage,
                    static_cast<unsigned long>(snapshot.request_id), snapshot.agent_busy,
                    static_cast<unsigned>(snapshot.games.size()),
                    static_cast<unsigned long>(esp_get_free_heap_size()),
                    static_cast<unsigned long>(esp_get_minimum_free_heap_size()),
                    snapshot.voice_status.c_str(), snapshot.notice.c_str(), snapshot.agent_text.c_str());
        TaskStatus();
        std::printf("firmware=%s reset_reason=%d uptime_ms=%llu\n", esp_app_get_description()->version,
                    static_cast<int>(esp_reset_reason()),
                    static_cast<unsigned long long>(esp_timer_get_time() / 1000));
    }
    else if (command == "page")
    {
        constexpr const char* names[] = {"home", "voice", "agent", "games", "game", "settings", "diag"};
        for (unsigned i = 0; i < 7; ++i)
        {
            if (arguments == names[i])
            {
                Report(app.Page_Navigate(static_cast<Mode>(i)));
                return;
            }
        }
        Report(Result::Fail(Error::Invalid, "Unknown page"));
    }
    else if (command == "key")
    {
        std::istringstream input(arguments);
        std::string name;
        int pressed = 1;
        input >> name >> pressed;
        constexpr const char* names[] = {"up", "down", "left", "right", "confirm", "back"};
        for (unsigned i = 0; i < 6; ++i)
        {
            if (name == names[i])
            {
                app.Key_Input(static_cast<Key>(i), pressed != 0);
                return;
            }
        }
        Report(Result::Fail(Error::Invalid, "Unknown key"));
    }
    else if (command == "wifi_config")
    {
        cJSON* json = cJSON_Parse(arguments.c_str());
        auto* ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
        auto* password = cJSON_GetObjectItemCaseSensitive(json, "password");
        if (!cJSON_IsString(ssid) || !cJSON_IsString(password))
        {
            Report(Result::Fail(Error::Invalid, "Expected JSON ssid and password strings"));
        }
        else
        {
            WifiConfig config{ssid->valuestring, password->valuestring};
            Report(app.Wifi_Configure(config));
            std::fill(config.password.begin(), config.password.end(), '\0');
        }
        cJSON_Delete(json);
    }
    else if (command == "agent_config")
    {
        cJSON* json = cJSON_Parse(arguments.c_str());
        auto field = [&](const char* name)
        {
            auto* item = cJSON_GetObjectItemCaseSensitive(json, name);
            return cJSON_IsString(item) ? std::string(item->valuestring) : std::string();
        };
        auto config = AgentConfig{field("url"), field("model"), field("key")};
        cJSON_Delete(json);
        Report(app.Agent_Configure(config));
        std::fill(config.key.begin(), config.key.end(), '\0');
    }
    else
    {
        Report(Result::Fail(Error::Invalid, "Unknown command; use help"));
    }
}
} // namespace mosicrokey
