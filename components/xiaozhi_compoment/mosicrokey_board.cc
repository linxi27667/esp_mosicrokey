#include "xiaozhi_bridge.hpp"
#include "mosicrokey/board_config.hpp"
#include "application.h"
#include "wifi_board.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "display.h"
#include "led/single_led.h"
#include <driver/i2c_master.h>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstdio>

extern "C" bool mosicrokey_xiaozhi_focus_allowed();
namespace {
std::atomic<uint32_t> input_frames{0},output_frames{0};
std::atomic<int> input_peak{0};
std::atomic<bool> codec_input_active{false},codec_output_active{false};
class ProductCodec final : public Es8311AudioCodec {
public:
    using Es8311AudioCodec::Es8311AudioCodec;
    bool InputData(std::vector<int16_t>& data) override {
        if(!mosicrokey_xiaozhi_focus_allowed()) return false;
        auto result=Es8311AudioCodec::InputData(data);
        if(result) {
            int peak=0; for(auto sample:data) peak=std::max(peak,std::abs(static_cast<int>(sample)));
            input_peak=peak; ++input_frames;
        }
        return result;
    }
    void OutputData(std::vector<int16_t>& data) override {
        if(!mosicrokey_xiaozhi_focus_allowed()) return;
        Es8311AudioCodec::OutputData(data); ++output_frames;
    }
    void EnableInput(bool enable) override {
        enable=enable && mosicrokey_xiaozhi_focus_allowed();
        Es8311AudioCodec::EnableInput(enable); codec_input_active=enable;
    }
    void EnableOutput(bool enable) override {
        enable=enable && mosicrokey_xiaozhi_focus_allowed();
        Es8311AudioCodec::EnableOutput(enable); codec_output_active=enable;
    }
};
}
std::string mosicrokey::CodecDiagnostics() {
    char result[180];
    std::snprintf(result,sizeof(result),"input_enabled=%d output_enabled=%d input_frames=%lu output_frames=%lu input_peak=%d",
        codec_input_active.load(),codec_output_active.load(),static_cast<unsigned long>(input_frames.load()),
        static_cast<unsigned long>(output_frames.load()),input_peak.load());
    return result;
}

class ProductDisplay final : public Display {
public:
    ProductDisplay() { width_ = 160; height_ = 128; }
    void SetStatus(const char* status) override {
        std::lock_guard lock(mutex_); status_ = status ? status : "";
        if (auto* observer = mosicrokey::XiaozhiEvents()) observer->Voice(status_, "");
    }
    void ShowNotification(const char* text, int = 3000) override {
        if (auto* observer = mosicrokey::XiaozhiEvents()) observer->Notice(text ? text : "");
    }
    void SetEmotion(const char*) override {}
    void SetChatMessage(const char*, const char* content) override {
        std::lock_guard lock(mutex_);
        if (auto* observer = mosicrokey::XiaozhiEvents()) observer->Voice(status_, content ? content : "");
    }
    void UpdateStatusBar(bool = false) override {}
    void SetPowerSaveMode(bool) override {}
    void ClearChatMessages() override { SetChatMessage("system", ""); }
protected:
    bool Lock(int) override { return true; }
    void Unlock() override {}
private:
    std::mutex mutex_;
    std::string status_;
};
namespace pins=mosicrokey::board::config;
constexpr gpio_num_t Gpio(int pin) {return static_cast<gpio_num_t>(pin);}
class MosicrokeyBoard final : public WifiBoard {
public:
    MosicrokeyBoard() : boot_(static_cast<gpio_num_t>(pins::boot)) {
        i2c_master_bus_config_t config{};
        config.i2c_port = I2C_NUM_0; config.sda_io_num = static_cast<gpio_num_t>(pins::i2c_sda); config.scl_io_num = static_cast<gpio_num_t>(pins::i2c_scl);
        config.clk_source = I2C_CLK_SRC_DEFAULT; config.glitch_ignore_cnt = 7; config.flags.enable_internal_pullup = true;
        ESP_ERROR_CHECK(i2c_new_master_bus(&config, &codec_bus_));
        boot_.OnClick([] { Application::GetInstance().ToggleChatState(); });
    }
    Display* GetDisplay() override { return &display_; }
    AudioCodec* GetAudioCodec() override {
        static ProductCodec codec(codec_bus_, I2C_NUM_0, pins::sample_rate, pins::sample_rate,
            Gpio(pins::i2s_mclk),Gpio(pins::i2s_bclk),Gpio(pins::i2s_ws),Gpio(pins::i2s_dout),Gpio(pins::i2s_din),Gpio(pins::pa),ES8311_CODEC_DEFAULT_ADDR);
        return &codec;
    }
    Led* GetLed() override { static SingleLed led(static_cast<gpio_num_t>(pins::rgb)); return &led; }
private:
    i2c_master_bus_handle_t codec_bus_ = nullptr;
    Button boot_;
    ProductDisplay display_;
};
DECLARE_BOARD(MosicrokeyBoard);
