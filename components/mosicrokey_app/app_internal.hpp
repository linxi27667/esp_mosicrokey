#pragma once
#include "mosicrokey/app.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mutex>
#include <atomic>

namespace mosicrokey
{
// 私有状态留在组件内部；仅 APP 业务与对应任务共享，不暴露给 HAL/UI。
struct App::Impl
{
    enum class CommandType
    {
        Navigate,
        Key,
        Encoder,
        Remount
    };
    struct Command
    {
        CommandType type;
        int value;
        bool pc_input = false;
        uint32_t pc_session = 0;
        int64_t submitted_us = 0;
    };
    App& owner;
    mutable std::mutex mutex;
    Snapshot state;
    std::mutex storage_control;
    IXiaozhiService* voice = nullptr;
    IAgentService* agent = nullptr;
    IGameLibrary* library = nullptr;
    IGameRuntime* runtime = nullptr;
    QueueHandle_t commands = nullptr;
    TaskHandle_t actor = nullptr, game_task = nullptr;
    std::atomic<int> game_generation{0}, pending_fault{0};
    std::atomic<bool> library_refresh_pending{false};
    Mode device_return = Mode::Home;
    uint32_t pc_session = 0;
    int64_t pc_heartbeat_us = 0;
    int64_t pc_sent_us = 0;

    explicit Impl(App& app) : owner(app) {}
    Result Command_Submit(CommandType type, int value = 0);
    void Storage_Check_Result(const Result& result);
    void Game_Library_Refresh();
    void Page_Change_Process(Mode mode);
    void Key_Press_Process(Key key);
    void Encoder_Rotate_Process(int detents);
    void Pc_Action_Submit(const char* action, int amount = 1);
    void Pc_Link_Check();
    void Command_Process(const Command& command);
    void Navigation_Update_Process(unsigned tick);
    bool Game_Update_Process(float dt, std::vector<uint16_t>& frame);
    Result App_Task_Create();
    static void App_Task(void* context);
    static void Game_Task(void* context);
};
} // namespace mosicrokey
