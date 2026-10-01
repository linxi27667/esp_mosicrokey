#include "mosicrokey/xiaozhi.hpp"
#include "xiaozhi_bridge.hpp"
#include "application.h"
#include "board.h"
#include "wifi_board.h"
#include <ssid_manager.h>
#include <esp_system.h>
#include <esp_log.h>
#include <algorithm>
#include <atomic>

namespace {
std::atomic<bool> foreground{false};
mosicrokey::IEvents* events = nullptr; // Owned by the process-lifetime composition root.
}
extern "C" bool mosicrokey_xiaozhi_focus_allowed() { return foreground.load(); }

void Application::PauseConversation() {
    Schedule([this] {
        bool testing=GetDeviceState()==kDeviceStateAudioTesting;
        pending_listening_start_ = false;
        audio_service_.EnableAudioTesting(false);
        StopNotification();
        if (protocol_ && protocol_->IsAudioChannelOpened()) AbortSpeaking(kAbortReasonNone);
        if (protocol_ && protocol_->IsAudioChannelOpened()) protocol_->CloseAudioChannel();
        audio_service_.EnableVoiceProcessing(false);
        audio_service_.EnableWakeWordDetection(false);
        audio_service_.ResetDecoder();
        auto* codec=Board::GetInstance().GetAudioCodec();
        codec->EnableInput(false);
        codec->EnableOutput(false);
        auto state = GetDeviceState();
        if(testing) SetDeviceState(kDeviceStateWifiConfiguring);
        if (state == kDeviceStateListening || state == kDeviceStateSpeaking || state == kDeviceStateConnecting || state == kDeviceStateNotifying)
            SetDeviceState(kDeviceStateIdle);
    });
}
namespace mosicrokey {
IEvents* XiaozhiEvents() { return events; }
class VoiceAdapter final : public IXiaozhiService {
public:
    void DiagnoseAudio() override {
        Application::GetInstance().Schedule([] {
            auto& app=Application::GetInstance(); auto& audio=app.GetAudioService();
            ESP_LOGI("mosi_audio","focus=%d state=%d wake=%d processing=%d playback_idle=%d %s",
                foreground.load(),static_cast<int>(app.GetDeviceState()),audio.IsWakeWordRunning(),
                audio.IsAudioProcessorRunning(),audio.IsPlaybackIdle(),CodecDiagnostics().c_str());
        });
    }
    void SetForeground(bool enabled) override {
        foreground = enabled;
        auto& app = Application::GetInstance();
        if (!enabled) app.PauseConversation();
        else app.Schedule([&app] { if (app.GetDeviceState() == kDeviceStateIdle) app.GetAudioService().EnableWakeWordDetection(true); });
    }
    void ToggleChat() override { Application::GetInstance().ToggleChatState(); }
    void ConfigureWifi() override {
        Application::GetInstance().Schedule([] { static_cast<WifiBoard&>(Board::GetInstance()).EnterWifiConfigMode(); });
    }
    Result ConfigureWifi(const WifiConfig& config) override {
        if (config.ssid.empty() || config.ssid.size() > 32 || config.ssid.find('\0') != std::string::npos ||
            config.password.find('\0') != std::string::npos ||
            (!config.password.empty() && (config.password.size() < 8 || config.password.size() > 63)))
            return Result::Fail(Error::Invalid,"SSID must be 1..32 bytes; password empty or 8..63 bytes");
        // Run under the sole Wi-Fi owner's task. Restart after saving so station/AP
        // callbacks cannot race a credential update or recursively start station mode.
        Application::GetInstance().Schedule([saved=config]() mutable {
            SsidManager::GetInstance().AddSsid(saved.ssid,saved.password);
            std::fill(saved.password.begin(),saved.password.end(),'\0');
            if (auto* observer=XiaozhiEvents()) observer->Notice("Wi-Fi 配置已保存，正在重启");
            esp_restart();
        });
        return {};
    }
};
std::unique_ptr<IXiaozhiService> CreateXiaozhi(IEvents& observer) {
    events = &observer;
    return std::make_unique<VoiceAdapter>();
}
void InitializeXiaozhi() {
    // The managed provisioning library logs SmartConfig passwords at INFO.
    esp_log_level_set("WifiConfigurationAp",ESP_LOG_WARN);
    Application::GetInstance().Initialize();
}
void RunXiaozhi() {Application::GetInstance().Run();}
}
