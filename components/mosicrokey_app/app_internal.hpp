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
        Remount
    };
    struct Command
    {
        CommandType type;
        int value;
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

    explicit Impl(App& app) : owner(app) {}
    Result Command_Submit(CommandType type, int value = 0);
    void Storage_Check_Result(const Result& result);
    void Game_Library_Refresh();
    void Page_Change_Process(Mode mode);
    void Key_Press_Process(Key key);
    void Command_Process(const Command& command);
    void Navigation_Update_Process(unsigned tick);
    bool Game_Update_Process(float dt, std::vector<uint16_t>& frame);
    Result App_Task_Create();
    static void App_Task(void* context);
    static void Game_Task(void* context);
};
} // namespace mosicrokey
