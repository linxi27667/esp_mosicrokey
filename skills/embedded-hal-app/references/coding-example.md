# 直接 HAL → APP → Task 范式

下面是 ESP-IDF 单实例 LED 的完整最小链路。`BOARD_LED_GPIO` 由实际板级配置提供；不要将示例引脚当成任何板卡默认值。所有函数在任务上下文执行，只有创建任务调用一次。HAL 无 APP 依赖，APP 拥有闪烁状态；Task 仅调用业务并等待。

```c
// config/board_config.h
#pragma once
#include "sdkconfig.h"
#define BOARD_LED_GPIO CONFIG_PRODUCT_LED_GPIO
```

```c
// hardware/hal_led.h
#pragma once
#include <stdbool.h>
#include "esp_err.h"
// 启动时调用；同步完成；单任务拥有，无内部锁。
esp_err_t Hal_Led_Init(void);
// 不等待；GPIO 写入结果直接返回。
esp_err_t Hal_Led_Set(bool enabled);
```

```c
// hardware/hal_led.c
#include "hal_led.h"
#include "board_config.h"
#include "driver/gpio.h"

esp_err_t Hal_Led_Init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOARD_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t error = gpio_config(&config);
    if (error != ESP_OK)
    {
        return error;
    }
    return Hal_Led_Set(false);
}

esp_err_t Hal_Led_Set(bool enabled)
{
    return gpio_set_level(BOARD_LED_GPIO, enabled);
}
```

```c
// app/app_indicator.h
#pragma once
#include "esp_err.h"
esp_err_t Indicator_Init(void);
// 仅 Indicator_Task 调用；成功后才更新业务状态。
esp_err_t Indicator_Update_Process(void);
```

```c
// app/app_indicator.c
#include "app_indicator.h"
#include "hal_led.h"

static bool g_enabled;

esp_err_t Indicator_Init(void)
{
    g_enabled = false;
    return Hal_Led_Init();
}

esp_err_t Indicator_Update_Process(void)
{
    bool next = !g_enabled;
    esp_err_t error = Hal_Led_Set(next);
    if (error == ESP_OK)
    {
        g_enabled = next;
    }
    return error;
}
```

```c
// task/indicator_task.c
#include "app_indicator.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// 实际项目集中放 task_config.h；ESP-IDF 栈参数为字节。
static const unsigned g_stack_bytes = 2048;
static const unsigned g_priority = 1;

static void Indicator_Task(void *context)
{
    (void)context;
    TickType_t last_wake = xTaskGetTickCount();
    while (1)
    {
        esp_err_t error = Indicator_Update_Process();
        if (error != ESP_OK)
        {
            ESP_LOGW("indicator", "LED write failed: %s", esp_err_to_name(error));
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500));
    }
}

esp_err_t Indicator_Task_Create(void)
{
    esp_err_t error = Indicator_Init();
    if (error != ESP_OK)
    {
        return error;
    }
    return xTaskCreate(Indicator_Task, "indicator", g_stack_bytes, NULL,
                       g_priority, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
```

不再增加 `led_operations_t`、`HW_Gpio_Write(void *port)`、`ILed` 或工厂。遇到多 LED，优先枚举 ID + 静态配置表；只有真实独立实例需要有类型的设备 context。

异步 DMA、存储重挂、网络取消和音频具有更复杂的生命周期，按实际约束实现，不能套用本例“写完即成功”的简化条件。本项目对应实现见 `components/board_support/hardware/` 和 `components/mosicrokey_app/task/`。
