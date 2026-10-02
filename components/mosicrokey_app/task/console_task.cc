#include "mosicrokey/console.hpp"
#include "mosicrokey/hal_uart.hpp"
#include "mosicrokey/task_config.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_app_desc.h>
#include <cstdio>
#include <algorithm>

namespace mosicrokey
{
namespace
{
void Console_Task(void* context)
{
    auto& app = *static_cast<App*>(context);
    std::string line;
    line.reserve(6144);
    bool overflow = false;
    std::printf("MosiCrokey v%s. Send help; UART 115200 8N1, UTF-8, LF. Input echo disabled.\n",
                esp_app_get_description()->version);
    while (1)
    {
        uint8_t byte;
        size_t received = 0;
        auto error = hal::Hal_Uart_Read(&byte, 1, received, task_config::console_wait_ms);
        if (error != ESP_OK)
        {
            app.Notice(esp_err_to_name(error));
            vTaskDelay(pdMS_TO_TICKS(task_config::console_wait_ms));
            continue;
        }
        if (!received || byte == '\r')
        {
            continue;
        }
        if (byte == '\n')
        {
            if (overflow)
            {
                std::puts("mosi: Command exceeds 6144 bytes");
            }
            else if (!line.empty())
            {
                Console_Command_Process(app, line);
            }
            std::fill(line.begin(), line.end(), '\0');
            line.clear();
            overflow = false;
        }
        else if (byte == 8 || byte == 127)
        {
            if (!line.empty())
            {
                line.back() = '\0';
                line.pop_back();
            }
        }
        else if (!overflow)
        {
            if (line.size() < 6144)
            {
                line.push_back(byte);
            }
            else
            {
                overflow = true;
            }
        }
    }
}
} // namespace

Result Console_Task_Create(App& app)
{
    auto error = hal::Hal_Uart_Init();
    if (error != ESP_OK)
    {
        return Result::Fail(Error::Runtime, esp_err_to_name(error));
    }
    if (xTaskCreate(Console_Task, "mosi_console", task_config::console_stack_bytes, &app,
                    task_config::console_priority, nullptr) != pdPASS)
    {
        return Result::Fail(Error::Runtime, "Console allocation failed");
    }
    return {};
}
} // namespace mosicrokey
