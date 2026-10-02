#include "mosicrokey/app.hpp"
#include "mosicrokey/task_config.hpp"
#include "mosicrokey/board_config.hpp"
#include "mosicrokey/hal_key.hpp"
#include "mosicrokey/hal_encoder.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <array>

namespace mosicrokey
{
namespace
{
static_assert(pdMS_TO_TICKS(task_config::key_period_ms) > 0);
static_assert(board::config::keys.size() == static_cast<unsigned>(Key::EncoderPress) + 1);

void Key_Scan_Task(void* context)
{
    auto& app = *static_cast<App*>(context);
    std::array<hal::key_event_t, board::config::keys.size()> events;
    TickType_t last_wake = xTaskGetTickCount();
    while (1)
    {
        size_t count = 0;
        auto error = hal::Hal_Key_Scan(static_cast<uint32_t>(esp_timer_get_time() / 1000), events.data(),
                                       events.size(), count);
        if (error == ESP_OK)
        {
            for (size_t i = 0; i < count; ++i)
            {
                app.Key_Input(static_cast<Key>(events[i].id), events[i].pressed);
            }
        }
        else
        {
            app.Notice(esp_err_to_name(error));
        }
        int detents = 0;
        if (hal::Hal_Encoder_Read(detents) == ESP_OK && detents)
        {
            auto result = app.Encoder_Input(detents);
            if (!result)
            {
                app.Notice(result.message);
            }
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(task_config::key_period_ms));
    }
}
} // namespace

Result Key_Scan_Task_Create(App& app)
{
    auto error = hal::Hal_Key_Init();
    if (error != ESP_OK)
    {
        return Result::Fail(Error::Runtime, esp_err_to_name(error));
    }
    error = hal::Hal_Encoder_Init();
    if (error != ESP_OK)
    {
        // 编码器初始化失败仍可用八键导航。
        app.Notice(std::string("编码器: ") + esp_err_to_name(error));
    }
    if (xTaskCreate(Key_Scan_Task, "mkeys", task_config::key_stack_bytes, &app, task_config::key_priority,
                    nullptr) != pdPASS)
    {
        return Result::Fail(Error::Runtime, "Key task allocation failed");
    }
    return {};
}
} // namespace mosicrokey
