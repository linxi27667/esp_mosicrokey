#include "mosicrokey/hal_encoder.hpp"
#include "mosicrokey/board_config.hpp"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

namespace mosicrokey::hal
{
namespace
{
portMUX_TYPE g_encoder_lock = portMUX_INITIALIZER_UNLOCKED;
bool g_ready = false;
unsigned g_phase = 0;
int g_edges = 0, g_detents = 0;
constexpr int g_step[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};

unsigned Encoder_Phase_Read()
{
    return (gpio_get_level(static_cast<gpio_num_t>(board::config::encoder_a)) << 1) |
           gpio_get_level(static_cast<gpio_num_t>(board::config::encoder_b));
}

void Encoder_Edge_Process(void*)
{
    portENTER_CRITICAL_ISR(&g_encoder_lock);
    unsigned phase = Encoder_Phase_Read();
    if ((phase ^ g_phase) == 3)
    {
        g_edges = 0;
    }
    else
    {
        g_edges += g_step[(g_phase << 2) | phase];
        constexpr int edges = board::config::encoder_edges_per_detent;
        if (g_edges >= edges || g_edges <= -edges)
        {
            int direction = g_edges > 0 ? 1 : -1;
            if (g_detents > -1024 && g_detents < 1024)
            {
                g_detents += direction;
            }
            g_edges = 0;
        }
    }
    g_phase = phase;
    portEXIT_CRITICAL_ISR(&g_encoder_lock);
}
} // namespace

esp_err_t Hal_Encoder_Init()
{
    if (g_ready)
    {
        return ESP_OK;
    }
    gpio_config_t config{};
    config.pin_bit_mask = (1ULL << board::config::encoder_a) | (1ULL << board::config::encoder_b);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.intr_type = GPIO_INTR_ANYEDGE;
    esp_err_t error = gpio_config(&config);
    if (error != ESP_OK)
    {
        return error;
    }
    error = gpio_install_isr_service(0);
    bool owns_service = error == ESP_OK;
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
    {
        return error;
    }
    g_phase = Encoder_Phase_Read();
    g_edges = g_detents = 0;
    auto pin_a = static_cast<gpio_num_t>(board::config::encoder_a);
    auto pin_b = static_cast<gpio_num_t>(board::config::encoder_b);
    error = gpio_isr_handler_add(pin_a, Encoder_Edge_Process, nullptr);
    if (error == ESP_OK)
    {
        error = gpio_isr_handler_add(pin_b, Encoder_Edge_Process, nullptr);
        if (error != ESP_OK)
        {
            gpio_isr_handler_remove(pin_a);
        }
    }
    if (error != ESP_OK)
    {
        if (owns_service)
        {
            gpio_uninstall_isr_service();
        }
        return error;
    }
    g_ready = true;
    return ESP_OK;
}

esp_err_t Hal_Encoder_Read(int& detents)
{
    detents = 0;
    if (!g_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }
    portENTER_CRITICAL(&g_encoder_lock);
    detents = g_detents;
    g_detents = 0;
    portEXIT_CRITICAL(&g_encoder_lock);
    return ESP_OK;
}
} // namespace mosicrokey::hal
