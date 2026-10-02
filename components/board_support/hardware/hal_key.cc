#include "mosicrokey/hal_key.hpp"
#include "mosicrokey/board_config.hpp"
#include <driver/gpio.h>
#include <array>

namespace mosicrokey::hal
{
namespace
{
struct key_state_t
{
    bool stable = false;
    bool candidate = false;
    uint32_t candidate_since_ms = 0;
};
std::array<key_state_t, board::config::keys.size()> g_keys{};
bool g_ready = false;
} // namespace

esp_err_t Hal_Key_Init()
{
    if (g_ready)
    {
        return ESP_OK;
    }
    gpio_config_t config{};
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    for (int pin : board::config::keys)
    {
        config.pin_bit_mask |= 1ULL << pin;
    }
    esp_err_t error = gpio_config(&config);
    if (error == ESP_OK)
    {
        g_keys = {};
        g_ready = true;
    }
    return error;
}

esp_err_t Hal_Key_Read(unsigned id, bool& pressed)
{
    if (!g_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (id >= board::config::keys.size())
    {
        return ESP_ERR_INVALID_ARG;
    }
    pressed = gpio_get_level(static_cast<gpio_num_t>(board::config::keys[id])) == 0;
    return ESP_OK;
}

//=====稳定电平转为一次按键事件=====
esp_err_t Hal_Key_Scan(uint32_t now_ms, key_event_t* events, size_t capacity, size_t& count)
{
    count = 0;
    if (!g_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (!events || capacity < g_keys.size())
    {
        return ESP_ERR_INVALID_ARG;
    }
    for (unsigned id = 0; id < g_keys.size(); ++id)
    {
        bool pressed = false;
        Hal_Key_Read(id, pressed);
        auto& state = g_keys[id];
        if (pressed != state.candidate)
        {
            state.candidate = pressed;
            state.candidate_since_ms = now_ms;
        }
        if (state.stable != state.candidate &&
            static_cast<uint32_t>(now_ms - state.candidate_since_ms) >= board::config::key_debounce_ms)
        {
            state.stable = state.candidate;
            events[count++] = {id, state.stable};
        }
    }
    return ESP_OK;
}
} // namespace mosicrokey::hal
