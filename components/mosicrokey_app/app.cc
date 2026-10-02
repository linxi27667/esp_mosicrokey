#include "mosicrokey/app.hpp"
#include "mosicrokey/focus.hpp"
#include "mosicrokey/hal_storage.hpp"
#include "mosicrokey/task_config.hpp"
#include "app_internal.hpp"
#include "mosicrokey/ui.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_netif.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <cJSON.h>
#include <algorithm>
#include <mutex>
#include <atomic>

namespace mosicrokey
{
namespace
{
std::string Bounded(const std::string& text, size_t size = 4096)
{
    if (text.size() <= size)
    {
        return text;
    }
    while (size && (static_cast<unsigned char>(text[size]) & 0xc0) == 0x80)
    {
        --size;
    }
    return text.substr(0, size) + "…";
}
} // namespace
// The queue copies the payload synchronously; local storage avoids shared command buffers.
Result App::Impl::Command_Submit(CommandType type, int value)
{
    Command item{type, value};
    if (type == CommandType::Key || type == CommandType::Encoder)
    {
        std::lock_guard lock(mutex);
        item.pc_input = state.mode == Mode::Computer;
        item.pc_session = state.pc_connected ? pc_session : 0;
        item.submitted_us = esp_timer_get_time();
    }
    return commands && xQueueSend(commands, &item, 0) == pdPASS ? Result{}
                                                                : Result::Fail(Error::Busy, "操作队列已满");
}
void App::Impl::Storage_Check_Result(const Result& result)
{
    if (result.error == Error::Storage)
    {
        hal::Hal_Storage_Mark_Failed();
        std::lock_guard lock(mutex);
        state.storage = false;
    }
}
void App::Impl::Game_Library_Refresh()
{
    std::vector<GameInfo> games;
    auto result = library->List(games);
    Storage_Check_Result(result);
    {
        std::lock_guard lock(mutex);
        state.games = std::move(games);
        state.storage = hal::Hal_Storage_Is_Mounted();
        if (!result)
        {
            state.notice = Bounded(result.message, 256);
        }
        if (state.mode == Mode::Library)
        {
            state.selection =
                std::min(state.selection, std::max(0, static_cast<int>(state.games.size()) - 1));
        }
    }
}
void App::Impl::Page_Change_Process(Mode mode)
{
    Mode old;
    {
        std::lock_guard lock(mutex);
        old = state.mode;
        state.notice.clear();
        if (mode == Mode::Computer && old != Mode::Computer)
        {
            device_return = old == Mode::Game ? Mode::Library : old;
        }
    }
    auto focus = ChangeFocus(old, mode);
    if (focus.release_audio)
    {
        voice->SetForeground(false);
    }
    if (focus.stop_game)
    {
        ++game_generation;
        runtime->Stop();
    }
    if (focus.acquire_audio)
    {
        voice->SetForeground(true);
    }
    if (mode == Mode::Library)
    {
        Game_Library_Refresh();
    }
    {
        std::lock_guard lock(mutex);
        state.mode = mode;
        state.selection = 0;
    }
}
//=====操作命令与前台状态机=====
void App::Impl::Command_Process(const Command& command)
{
    bool local_key =
        command.type == CommandType::Key && (command.value == static_cast<int>(Key::SwitchMode) ||
                                             command.value == static_cast<int>(Key::Function));
    if ((command.type == CommandType::Key || command.type == CommandType::Encoder) && !local_key)
    {
        std::lock_guard lock(mutex);
        if (command.pc_input != (state.mode == Mode::Computer))
        {
            state.pc_result = "模式切换后已丢弃旧输入";
            return;
        }
        if (command.pc_input &&
            (!state.pc_connected || !command.pc_session || command.pc_session != pc_session ||
             esp_timer_get_time() - command.submitted_us > 500000))
        {
            state.pc_result = "电脑操作已过期";
            return;
        }
    }
    switch (command.type)
    {
    case CommandType::Navigate:
        Page_Change_Process(static_cast<Mode>(command.value));
        break;
    case CommandType::Key:
        Key_Press_Process(static_cast<Key>(command.value));
        break;
    case CommandType::Encoder:
        Encoder_Rotate_Process(command.value);
        break;
    case CommandType::Remount:
    {
        std::lock_guard operation(storage_control);
        auto snapshot = owner.State_Read();
        if (snapshot.agent_busy || runtime->Running())
        {
            owner.Notice("请先取消 Agent 或退出游戏再重挂 TF");
        }
        else
        {
            auto error = hal::Hal_Storage_Mount();
            owner.Notice(error == ESP_OK ? "TF 已重新挂载" : std::string("TF: ") + esp_err_to_name(error));
            Game_Library_Refresh();
        }
        break;
    }
    }
}

void App::Impl::Navigation_Update_Process(unsigned tick)
{
    Pc_Link_Check();
    if (library_refresh_pending.exchange(false))
    {
        Game_Library_Refresh();
    }
    int fault = pending_fault.exchange(0);
    if (fault && fault == game_generation && owner.State_Read().mode == Mode::Game)
    {
        auto reason = owner.State_Read().notice;
        Page_Change_Process(Mode::Library);
        owner.Notice(reason);
    }
    if (tick % 20 == 0)
    {
        esp_netif_ip_info_t ip{};
        auto* network = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        bool wifi = network && esp_netif_get_ip_info(network, &ip) == ESP_OK && ip.ip.addr != 0;
        std::lock_guard lock(mutex);
        state.wifi = wifi;
        state.storage = hal::Hal_Storage_Is_Mounted();
    }
    ui::Update(owner.State_Read());
    if (tick % 600 == 0)
    {
        ESP_LOGI(
            "mosi_app",
            "heap_internal=%lu min_internal=%lu psram=%lu min_psram=%lu stack_app=%u stack_game=%u",
            static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
            static_cast<unsigned long>(
                heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
            static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
            static_cast<unsigned long>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
            static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
            static_cast<unsigned>(uxTaskGetStackHighWaterMark(game_task)));
    }
}

//=====单帧游戏业务；不等待网络/存储=====
bool App::Impl::Game_Update_Process(float dt, std::vector<uint16_t>& frame)
{
    bool foreground;
    {
        std::lock_guard lock(mutex);
        foreground = state.mode == Mode::Game;
    }
    if (!foreground || !runtime->Running())
    {
        return false;
    }
    int generation = game_generation;
    auto result = runtime->Tick(dt, frame);
    if (result)
    {
        ui::Frame(frame);
        return true;
    }
    if (result.error != Error::NotReady && generation == game_generation)
    {
        owner.Notice("游戏结束: " + result.message);
        pending_fault = generation;
    }
    return false;
}

App::App() : impl_(std::make_unique<Impl>(*this)) {}
App::~App() = default; // Product services live for the lifetime of the firmware.
Result App::System_Start(IXiaozhiService& voice, IAgentService& agent, IGameLibrary& library,
                         IGameRuntime& runtime)
{
    if (impl_->commands)
    {
        return Result::Fail(Error::Busy, "App already started");
    }
    impl_->voice = &voice;
    impl_->agent = &agent;
    impl_->library = &library;
    impl_->runtime = &runtime;
    impl_->commands = xQueueCreate(task_config::command_queue_capacity, sizeof(Impl::Command));
    if (!impl_->commands)
    {
        return Result::Fail(Error::Runtime, "App queue allocation failed");
    }
    voice.SetForeground(false);
    auto result = impl_->App_Task_Create();
    if (!result)
    {
        vQueueDelete(impl_->commands);
        impl_->commands = nullptr;
    }
    return result;
}
void App::Key_Input(Key key, bool pressed)
{
    if (static_cast<unsigned>(key) > static_cast<unsigned>(Key::EncoderPress))
    {
        Notice("无效按键");
        return;
    }
    Mode mode;
    bool confirm_pressed = false;
    {
        std::lock_guard lock(impl_->mutex);
        auto& state = impl_->state;
        unsigned bit = 1u << static_cast<unsigned>(key);
        state.key_mask = pressed ? state.key_mask | bit : state.key_mask & ~bit;
        ++state.input_events;
        constexpr const char* names[] = {"K1 上",   "K2 下",   "K3 左",   "K4 右",   "K5 确认",
                                         "K6 返回", "K7 功能", "K8 模式", "旋钮按压"};
        state.last_input = std::string(names[static_cast<unsigned>(key)]) + (pressed ? " 按下" : " 松开");
        mode = state.mode;
        confirm_pressed = state.key_mask & ((1u << static_cast<unsigned>(Key::Confirm)) |
                                            (1u << static_cast<unsigned>(Key::EncoderPress)));
    }
    if (mode == Mode::Game && (key <= Key::Confirm || key == Key::EncoderPress))
    {
        bool confirm = key == Key::Confirm || key == Key::EncoderPress;
        impl_->runtime->Input(confirm ? Key::Confirm : key, confirm ? confirm_pressed : pressed);
        return;
    }
    if (pressed)
    {
        auto result = impl_->Command_Submit(Impl::CommandType::Key, static_cast<int>(key));
        if (!result)
        {
            Notice(result.message);
        }
    }
}
Result App::Page_Navigate(Mode mode)
{
    if (mode == Mode::Game || static_cast<unsigned>(mode) > static_cast<unsigned>(Mode::DisplayTest))
    {
        return Result::Fail(Error::Invalid, "Start games from library");
    }
    return impl_->Command_Submit(Impl::CommandType::Navigate, static_cast<int>(mode));
}
Result App::Agent_Ask(const std::string& text, AgentIntent intent)
{
    std::lock_guard operation(impl_->storage_control);
    uint32_t request = 0;
    auto result = impl_->agent->Submit(text, request, intent);
    if (!result)
    {
        Notice(result.message);
    }
    return result;
}
Result App::Agent_Configure(const AgentConfig& config)
{
    return impl_->agent->Configure(config);
}
Result App::Agent_Cancel()
{
    auto snapshot = State_Read();
    auto result = impl_->agent->Cancel(snapshot.request_id);
    if (!result)
    {
        Notice(result.message);
    }
    return result;
}
Result App::Storage_Remount()
{
    return impl_->Command_Submit(Impl::CommandType::Remount);
}
Result App::Game_Install_Example(const std::string& lua)
{
    std::lock_guard operation(impl_->storage_control);
    if (State_Read().agent_busy)
    {
        return Result::Fail(Error::Busy, "Agent busy");
    }
    auto result = impl_->library->ImportExample(lua);
    impl_->library->DiscardStaging();
    impl_->Storage_Check_Result(result);
    impl_->library_refresh_pending = true;
    return result;
}
Result App::Game_Preview_Example(const std::string& lua)
{
    std::lock_guard operation(impl_->storage_control);
    if (State_Read().agent_busy)
    {
        return Result::Fail(Error::Busy, "Agent busy");
    }
    auto* json = cJSON_CreateObject();
    if (!json)
    {
        return Result::Fail(Error::Runtime, "Preview allocation failed");
    }
    bool fields = cJSON_AddStringToObject(json, "id", "snake-demo") &&
                  cJSON_AddStringToObject(json, "title", "贪吃蛇") &&
                  cJSON_AddStringToObject(json, "source", lua.c_str());
    char* request = fields ? cJSON_PrintUnformatted(json) : nullptr;
    cJSON_Delete(json);
    if (!request)
    {
        return Result::Fail(Error::Runtime, "Preview allocation failed");
    }
    std::string output;
    auto result = impl_->library->Tool("game_preview", request, output);
    cJSON_free(request);
    if (result)
    {
        impl_->library_refresh_pending = true;
    }
    return result;
}
void App::Wifi_Configure()
{
    impl_->voice->ConfigureWifi();
}
Result App::Wifi_Configure(const WifiConfig& config)
{
    return impl_->voice->ConfigureWifi(config);
}
void App::Audio_Diagnose()
{
    impl_->voice->DiagnoseAudio();
}
Result App::Game_Run_Self_Test(const std::string& source, std::string& report)
{
    return impl_->runtime->SelfTest(source, report);
}
Result App::Game_Capture_Frame(std::vector<uint16_t>& frame) const
{
    return impl_->runtime->CaptureFrame(frame);
}
Result App::Game_Read_Source(const std::string& id, unsigned offset, std::string& output) const
{
    // 源码读取可能访问 TF；与卸载/重挂使用同一业务操作锁。
    std::lock_guard operation(impl_->storage_control);
    if (id.empty() || id.size() > 32 || offset > 65536)
    {
        return Result::Fail(Error::Invalid, "Invalid game source query");
    }
    auto* query = cJSON_CreateObject();
    if (!query)
    {
        return Result::Fail(Error::Runtime, "Source query allocation failed");
    }
    cJSON_AddStringToObject(query, "id", id.c_str());
    cJSON_AddNumberToObject(query, "offset", offset);
    char* json = cJSON_PrintUnformatted(query);
    cJSON_Delete(query);
    if (!json)
    {
        return Result::Fail(Error::Runtime, "Source query allocation failed");
    }
    auto result = impl_->library->Tool("game_list", json, output);
    cJSON_free(json);
    return result;
}
Snapshot App::State_Read() const
{
    std::lock_guard lock(impl_->mutex);
    return impl_->state;
}
void App::Voice(const std::string& status, const std::string& text)
{
    std::lock_guard lock(impl_->mutex);
    impl_->state.voice_status = Bounded(status, 96);
    if (!text.empty())
    {
        impl_->state.voice_text = Bounded(text);
    }
}
void App::Agent(uint32_t request, bool busy, const std::string& text)
{
    {
        std::lock_guard lock(impl_->mutex);
        if (request < impl_->state.request_id)
        {
            return;
        }
        impl_->state.request_id = request;
        impl_->state.agent_busy = busy;
        impl_->state.agent_text = Bounded(text);
    }
    ESP_LOGI("mosi_agent", "request=%lu busy=%d", static_cast<unsigned long>(request), busy);
    if (!busy)
    {
        impl_->library_refresh_pending = true;
    }
}
void App::Notice(const std::string& text)
{
    std::lock_guard lock(impl_->mutex);
    impl_->state.notice = Bounded(text, 256);
}
void App::GameEnded(const std::string& reason)
{
    Notice("游戏结束: " + reason);
    impl_->pending_fault = impl_->game_generation.load();
}
} // namespace mosicrokey
