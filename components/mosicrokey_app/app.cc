#include "mosicrokey/app.hpp"
#include "mosicrokey/focus.hpp"
#include "mosicrokey/board.hpp"
#include "mosicrokey/ui.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_netif.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <cJSON.h>
#include <algorithm>
#include <mutex>
#include <atomic>

namespace mosicrokey {
namespace {
std::string Bounded(const std::string& text,size_t size=4096) {
    if(text.size()<=size) return text;
    while(size && (static_cast<unsigned char>(text[size])&0xc0)==0x80) --size;
    return text.substr(0,size)+"…";
}
}
struct App::Impl {
    enum class CommandType { Navigate, Key, Refresh, Remount };
    struct Command { CommandType type; int value; };
    App& owner; mutable std::mutex mutex; Snapshot state;
    std::mutex storage_control;
    IXiaozhiService* voice=nullptr; IAgentService* agent=nullptr;
    IGameLibrary* library=nullptr; IGameRuntime* runtime=nullptr;
    QueueHandle_t commands=nullptr;
    TaskHandle_t actor=nullptr, game_task=nullptr;
    std::atomic<int> game_generation{0}, pending_fault{0};
    explicit Impl(App& app):owner(app) {}
    // The queue copies the payload synchronously; local storage avoids shared command buffers.
    Result Submit(CommandType type,int value=0) {
        Command item{type,value};
        return commands && xQueueSend(commands,&item,0)==pdPASS ? Result{} : Result::Fail(Error::Busy,"操作队列已满");
    }
    void CheckStorage(const Result& result) {
        if(result.error==Error::Storage) {
            board::SetStorageFailed();
            std::lock_guard lock(mutex); state.storage=false;
        }
    }
    void Refresh() {
        std::vector<GameInfo> games; auto result=library->List(games);
        CheckStorage(result);
        {std::lock_guard lock(mutex); state.games=std::move(games); state.storage=board::CardMounted();
            if(!result) state.notice=Bounded(result.message,256);
            state.selection=std::min(state.selection,std::max(0,static_cast<int>(state.games.size())-1));}
    }
    void Navigate(Mode mode) {
        Mode old;
        {std::lock_guard lock(mutex); old=state.mode; state.notice.clear();}
        auto focus=ChangeFocus(old,mode);
        if(focus.release_audio) voice->SetForeground(false);
        if(focus.stop_game) {++game_generation; runtime->Stop();}
        if(focus.acquire_audio) voice->SetForeground(true);
        if(mode==Mode::Library) Refresh();
        {std::lock_guard lock(mutex); state.mode=mode; state.selection=0;}
    }
    void KeyPressed(Key key) {
        Snapshot snapshot=owner.Read();
        if(snapshot.mode==Mode::Library && !snapshot.notice.empty() && (key==Key::Back || key==Key::Confirm)) {
            std::lock_guard lock(mutex); state.notice.clear(); return;
        }
        if(key==Key::Back) {Navigate(BackDestination(snapshot.mode)); return;}
        if(key==Key::Up || key==Key::Down) {
            int limit=snapshot.mode==Mode::Home ? 5 : snapshot.mode==Mode::Library ? snapshot.games.size() : snapshot.mode==Mode::Settings ? 2 : 2048;
            std::lock_guard lock(mutex);
            state.selection=std::clamp(state.selection+(key==Key::Up ? -1 : 1),0,std::max(0,limit-1)); return;
        }
        if(key!=Key::Confirm) return;
        if(snapshot.mode==Mode::Home) Navigate(HomeDestination(snapshot.selection));
        else if(snapshot.mode==Mode::Xiaozhi) voice->ToggleChat();
        else if(snapshot.mode==Mode::Agent) owner.Cancel();
        else if(snapshot.mode==Mode::Settings) {if(snapshot.selection==0) owner.ConfigureWifi(); else owner.Remount();}
        else if(snapshot.mode==Mode::Library && snapshot.selection<static_cast<int>(snapshot.games.size())) {
            auto result=runtime->Start(snapshot.games[snapshot.selection]);
            CheckStorage(result);
            if(result) {++game_generation; Navigate(Mode::Game);} else owner.Notice(result.message);
        }
    }
    static void Actor(void* context) {
        auto& self=*static_cast<Impl*>(context); unsigned tick=0;
        for(;;) {
            Command item;
            if(xQueueReceive(self.commands,&item,pdMS_TO_TICKS(50))==pdPASS) {
                switch(item.type) {
                    case CommandType::Navigate: self.Navigate(static_cast<Mode>(item.value)); break;
                    case CommandType::Key: self.KeyPressed(static_cast<Key>(item.value)); break;
                    case CommandType::Refresh: self.Refresh(); break;
                    case CommandType::Remount: {
                        std::lock_guard operation(self.storage_control);
                        auto snapshot=self.owner.Read();
                        if(snapshot.agent_busy || self.runtime->Running()) self.owner.Notice("请先取消 Agent 或退出游戏再重挂 TF");
                        else {auto result=board::MountCard(); self.owner.Notice(result ? "TF 已重新挂载" : result.message); self.Refresh();}
                        break;
                    }
                }
            }
            int fault=self.pending_fault.exchange(0);
            if(fault && fault==self.game_generation && self.owner.Read().mode==Mode::Game) {
                auto reason=self.owner.Read().notice; self.Navigate(Mode::Library); self.owner.Notice(reason);
            }
            if(++tick%20==0) {
                esp_netif_ip_info_t ip{}; auto* network=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                bool wifi=network && esp_netif_get_ip_info(network,&ip)==ESP_OK && ip.ip.addr!=0;
                std::lock_guard lock(self.mutex); self.state.wifi=wifi; self.state.storage=board::CardMounted();
            }
            ui::Update(self.owner.Read());
            if(tick%600==0) ESP_LOGI("mosi_app","heap_internal=%lu min_internal=%lu psram=%lu min_psram=%lu stack_app=%u stack_game=%u",
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)),
                static_cast<unsigned long>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)),
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                static_cast<unsigned>(uxTaskGetStackHighWaterMark(self.game_task)));
        }
    }
    static void GameTask(void* context) {
        auto& self=*static_cast<Impl*>(context); std::vector<uint16_t> frame;
        TickType_t wake=xTaskGetTickCount(); int64_t last=esp_timer_get_time();
        unsigned frames=0; int64_t report=last;
        for(;;) {
            int64_t now=esp_timer_get_time();
            bool foreground;
            {std::lock_guard lock(self.mutex); foreground=self.state.mode==Mode::Game;}
            if(foreground && self.runtime->Running()) {
                int generation=self.game_generation;
                auto result=self.runtime->Tick(std::clamp((now-last)/1000000.f,0.001f,0.1f),frame);
                if(result) {ui::Frame(frame); ++frames;}
                else if(result.error!=Error::NotReady && generation==self.game_generation) {
                    self.owner.Notice("游戏结束: "+result.message); self.pending_fault=generation;
                }
            }
            last=now;
            if(now-report>=10000000) {if(frames) ESP_LOGI("mosi_game","fps=%.1f stack=%u",frames*1000000.0/(now-report),static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr))); frames=0; report=now;}
            vTaskDelayUntil(&wake,pdMS_TO_TICKS(33));
        }
    }
};
App::App():impl_(std::make_unique<Impl>(*this)) {}
App::~App()=default; // Product services live for the lifetime of the firmware.
Result App::Start(IXiaozhiService& voice,IAgentService& agent,IGameLibrary& library,IGameRuntime& runtime) {
    impl_->voice=&voice; impl_->agent=&agent; impl_->library=&library; impl_->runtime=&runtime;
    impl_->commands=xQueueCreate(12,sizeof(Impl::Command));
    if(!impl_->commands) return Result::Fail(Error::Runtime,"App queue allocation failed");
    voice.SetForeground(false);
    // Keep the bounded foreground VM away from Wi-Fi/TLS and display work on CPU0.
    // Priority 3 remains below upstream audio output (4) and input (8).
    if(xTaskCreatePinnedToCore(Impl::GameTask,"mosi_game",8192,impl_.get(),3,&impl_->game_task,1)!=pdPASS ||
       xTaskCreate(Impl::Actor,"mosi_app",8192,impl_.get(),2,&impl_->actor)!=pdPASS)
        return Result::Fail(Error::Runtime,"App task allocation failed");
    return {};
}
void App::Input(Key key,bool pressed) {
    if(Read().mode==Mode::Game && key!=Key::Back) {impl_->runtime->Input(key,pressed); return;}
    if(pressed) {auto result=impl_->Submit(Impl::CommandType::Key,static_cast<int>(key)); if(!result) Notice(result.message);}
}
Result App::Navigate(Mode mode) {
    if(mode==Mode::Game || static_cast<unsigned>(mode)>static_cast<unsigned>(Mode::Diagnostics)) return Result::Fail(Error::Invalid,"Start games from library");
    return impl_->Submit(Impl::CommandType::Navigate,static_cast<int>(mode));
}
Result App::Ask(const std::string& text,AgentIntent intent) {
    std::lock_guard operation(impl_->storage_control);
    uint32_t request=0; auto result=impl_->agent->Submit(text,request,intent); if(!result) Notice(result.message); return result;
}
Result App::ConfigureAgent(const AgentConfig& config) {return impl_->agent->Configure(config);}
Result App::Cancel() {auto snapshot=Read(); auto result=impl_->agent->Cancel(snapshot.request_id); if(!result) Notice(result.message); return result;}
Result App::Remount() {return impl_->Submit(Impl::CommandType::Remount);}
Result App::InstallExample(const std::string& lua) {
    std::lock_guard operation(impl_->storage_control);
    if(Read().agent_busy) return Result::Fail(Error::Busy,"Agent busy");
    auto result=impl_->library->ImportExample(lua); impl_->library->DiscardStaging();
    impl_->CheckStorage(result);
    impl_->Submit(Impl::CommandType::Refresh); return result;
}
void App::ConfigureWifi() {impl_->voice->ConfigureWifi();}
Result App::ConfigureWifi(const WifiConfig& config) {return impl_->voice->ConfigureWifi(config);}
void App::DiagnoseAudio() {impl_->voice->DiagnoseAudio();}
Result App::TestGameRuntime(const std::string& source,std::string& report) {return impl_->runtime->SelfTest(source,report);}
Result App::CaptureGameFrame(std::vector<uint16_t>& frame) const {return impl_->runtime->CaptureFrame(frame);}
Result App::ReadGameSource(const std::string& id,unsigned offset,std::string& output) const {
    if(id.empty() || id.size()>32 || offset>65536) return Result::Fail(Error::Invalid,"Invalid game source query");
    auto* query=cJSON_CreateObject();
    if(!query) return Result::Fail(Error::Runtime,"Source query allocation failed");
    cJSON_AddStringToObject(query,"id",id.c_str()); cJSON_AddNumberToObject(query,"offset",offset);
    char* json=cJSON_PrintUnformatted(query); cJSON_Delete(query);
    if(!json) return Result::Fail(Error::Runtime,"Source query allocation failed");
    auto result=impl_->library->Tool("game_list",json,output); cJSON_free(json); return result;
}
Snapshot App::Read() const {std::lock_guard lock(impl_->mutex); return impl_->state;}
void App::Voice(const std::string& status,const std::string& text) {
    std::lock_guard lock(impl_->mutex); impl_->state.voice_status=Bounded(status,96);
    if(!text.empty()) impl_->state.voice_text=Bounded(text);
}
void App::Agent(uint32_t request,bool busy,const std::string& text) {
    {std::lock_guard lock(impl_->mutex); if(request<impl_->state.request_id) return;
        impl_->state.request_id=request; impl_->state.agent_busy=busy; impl_->state.agent_text=Bounded(text);}
    ESP_LOGI("mosi_agent","request=%lu busy=%d",static_cast<unsigned long>(request),busy);
    if(!busy && impl_->commands) impl_->Submit(Impl::CommandType::Refresh);
}
void App::Notice(const std::string& text) {std::lock_guard lock(impl_->mutex); impl_->state.notice=Bounded(text,256);}
void App::GameEnded(const std::string& reason) {Notice("游戏结束: "+reason); impl_->pending_fault=impl_->game_generation.load();}
} // namespace mosicrokey
