#include "app_internal.hpp"
#include "mosicrokey/task_config.hpp"
#include <esp_timer.h>
#include <esp_log.h>
#include <algorithm>

namespace mosicrokey
{
void App::Impl::App_Task(void* context)
{
    auto& self = *static_cast<Impl*>(context);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    unsigned tick = 0;
    while (1)
    {
        Command command;
        if (xQueueReceive(self.commands, &command, pdMS_TO_TICKS(task_config::app_wait_ms)) == pdPASS)
        {
            self.Command_Process(command);
        }
        self.Navigation_Update_Process(++tick);
    }
}

void App::Impl::Game_Task(void* context)
{
    auto& self = *static_cast<Impl*>(context);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    std::vector<uint16_t> frame;
    TickType_t last_wake = xTaskGetTickCount();
    int64_t last_us = esp_timer_get_time(), report_us = last_us;
    unsigned frames = 0;
    while (1)
    {
        int64_t now_us = esp_timer_get_time();
        float dt = std::clamp((now_us - last_us) / 1000000.f, 0.001f, 0.1f);
        if (self.Game_Update_Process(dt, frame))
        {
            ++frames;
        }
        last_us = now_us;
        if (now_us - report_us >= 10000000)
        {
            if (frames)
            {
                ESP_LOGI("mosi_game", "fps=%.1f stack=%u", frames * 1000000.0 / (now_us - report_us),
                         static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            }
            frames = 0;
            report_us = now_us;
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(task_config::game_period_ms));
    }
}

Result App::Impl::App_Task_Create()
{
    // 游戏留 CPU1，低于音频输入/输出；UI 和模型请求留 CPU0。
    if (xTaskCreatePinnedToCore(Game_Task, "mosi_game", task_config::game_stack_bytes, this,
                                task_config::game_priority, &game_task, task_config::game_core) != pdPASS)
    {
        return Result::Fail(Error::Runtime, "Game task allocation failed");
    }
    if (xTaskCreate(App_Task, "mosi_app", task_config::app_stack_bytes, this, task_config::app_priority,
                    &actor) != pdPASS)
    {
        // Game_Task 尚在启动通知等待，未持有业务锁，可以安全回滚。
        vTaskDelete(game_task);
        game_task = nullptr;
        return Result::Fail(Error::Runtime, "App task allocation failed");
    }
    xTaskNotifyGive(game_task);
    xTaskNotifyGive(actor);
    return {};
}
} // namespace mosicrokey
