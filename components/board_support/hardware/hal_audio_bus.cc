#include "mosicrokey/hal_audio_bus.hpp"
#include "mosicrokey/board_config.hpp"

namespace mosicrokey::hal
{
namespace
{
i2c_master_bus_handle_t g_codec_bus = nullptr;
}

esp_err_t Hal_Audio_Bus_Init()
{
    if (g_codec_bus)
    {
        return ESP_OK;
    }
    i2c_master_bus_config_t config{};
    config.i2c_port = I2C_NUM_0;
    config.sda_io_num = static_cast<gpio_num_t>(board::config::i2c_sda);
    config.scl_io_num = static_cast<gpio_num_t>(board::config::i2c_scl);
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.flags.enable_internal_pullup = true;
    return i2c_new_master_bus(&config, &g_codec_bus);
}

i2c_master_bus_handle_t Hal_Audio_Bus_Get()
{
    return g_codec_bus;
}
} // namespace mosicrokey::hal
