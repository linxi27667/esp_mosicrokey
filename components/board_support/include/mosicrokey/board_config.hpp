#pragma once
#include <array>
namespace mosicrokey::board::config
{
// ESP32-S31-Function-CoreBoard-1. Board audio/BOOT/RGB/UART are fixed pins.
constexpr int i2c_scl = 50, i2c_sda = 51;
constexpr int i2s_mclk = 52, i2s_bclk = 53, i2s_din = 54, i2s_ws = 55, i2s_dout = 56, pa = 57;
constexpr int uart_tx = 58, uart_rx = 59, rgb = 60, boot = 61;
constexpr int sample_rate = 24000;
constexpr std::array<int, 6> keys = {38, 39, 40, 42, 49, 35};
constexpr int sd_d0 = 20, sd_d1 = 21, sd_d2 = 22, sd_d3 = 23, sd_clk = 24, sd_cmd = 25;
constexpr int spi_hz = 20000000, sd_khz = 20000;
constexpr int display_width = 160, display_height = 128;
constexpr int display_transfer_bytes = display_width * 20 * 2;
constexpr unsigned key_debounce_ms = 20;
constexpr int uart_baud = 115200;
constexpr char card_path[] = "/sdcard";
// TFT GPIOs/offsets/color inversion are set by the MosiCrokey Kconfig menu.
} // namespace mosicrokey::board::config
