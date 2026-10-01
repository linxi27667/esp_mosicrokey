#include "mosicrokey/app.hpp"
#include "mosicrokey/board.hpp"
#include "mosicrokey/ui.hpp"
#include "redaction.hpp"
#include "ram_history.hpp"
#include "completion.hpp"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <vector>
#include <memory>
#include <cstring>
#include <iostream>
#include <functional>
using namespace mosicrokey;
namespace {
std::atomic<bool> stopping{false};
struct Queue {unsigned capacity,size; std::mutex mutex; std::deque<std::vector<uint8_t>> items;};
std::vector<std::unique_ptr<Queue>> queues;
std::vector<std::thread> tasks;
unsigned checks=0;
std::atomic<unsigned> mounts{0};
std::atomic<bool> mounted{true};
void Check(bool valid,const char* name) {++checks; if(!valid) {std::cerr<<"FAIL: "<<name<<"\n"; std::exit(1);}}
void Wait(const std::function<bool()>& condition,const char* name) {
    auto deadline=esp_timer_get_time()+1000000;
    while(!condition() && esp_timer_get_time()<deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Check(condition(),name);
}
struct Voice:IXiaozhiService {
    std::atomic<bool> foreground{false}; unsigned toggles=0;
    void SetForeground(bool on) override {foreground=on;}
    void ToggleChat() override {++toggles;}
    void ConfigureWifi() override {}
    Result ConfigureWifi(const WifiConfig&) override {return {};}
    void DiagnoseAudio() override {}
};
struct Agent:IAgentService {
    App& app; std::atomic<bool> busy{false}; uint32_t request=0;
    AgentIntent last_intent=AgentIntent::Conversation;
    explicit Agent(App& owner):app(owner) {}
    Result Submit(const std::string&,uint32_t& id,AgentIntent intent) override {
        if(busy.exchange(true)) return Result::Fail(Error::Busy,"busy");
        last_intent=intent; id=++request; app.Agent(id,true,"pending"); return {};
    }
    Result Cancel(uint32_t) override {busy=false; app.Agent(request,false,"cancelled"); return {};}
    Result Configure(const AgentConfig&) override {return {};}
};
struct Games:IGameLibrary,IGameRuntime {
    std::atomic<bool> running{false},block{false},entered{false},fail{false},list_error{false};
    std::atomic<unsigned> inputs{0};
    Result List(std::vector<GameInfo>& list) override {
        if(list_error) return Result::Fail(Error::Storage,"Card read failed");
        list={{"snake","贪吃蛇","r00000001","virtual"}}; return {};
    }
    Result Tool(const std::string&,const std::string&,std::string&) override {return {};}
    Result ImportExample(const std::string&) override {return {};}
    void DiscardStaging() override {}
    Result Start(const GameInfo&) override {running=true; return {};}
    void Stop() override {running=false;}
    void Input(Key,bool) override {++inputs;}
    bool Running() const override {return running;}
    Result SelfTest(const std::string&,std::string&) override {return {};}
    Result CaptureFrame(std::vector<uint16_t>&) const override {return Result::Fail(Error::NotReady,"No game");}
    Result Tick(float,std::vector<uint16_t>& frame) override {
        entered=true; while(block && !stopping) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if(!running) return Result::Fail(Error::NotReady,"stopped");
        if(fail.exchange(false)) {running=false; return Result::Fail(Error::Runtime,"script error");}
        frame.resize(160*128); return {};
    }
};
}
bool MockStopping() {return stopping;}
void MockShutdown() {stopping=true; for(auto& task:tasks) task.join(); tasks.clear(); queues.clear();}
QueueHandle_t xQueueCreate(unsigned capacity,unsigned size) {
    auto queue=std::make_unique<Queue>(); queue->capacity=capacity; queue->size=size;
    auto* pointer=queue.get(); queues.push_back(std::move(queue)); return pointer;
}
BaseType_t xQueueSend(QueueHandle_t handle,const void* item,TickType_t) {
    auto& queue=*static_cast<Queue*>(handle); std::lock_guard lock(queue.mutex);
    if(queue.items.size()>=queue.capacity) return pdFALSE;
    auto& bytes=queue.items.emplace_back(queue.size); std::memcpy(bytes.data(),item,queue.size); return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t handle,void* item,TickType_t timeout) {
    auto& queue=*static_cast<Queue*>(handle); auto deadline=esp_timer_get_time()+timeout*1000;
    do {
        if(stopping) throw MockStop{};
        {std::lock_guard lock(queue.mutex); if(!queue.items.empty()) {std::memcpy(item,queue.items.front().data(),queue.size); queue.items.pop_front(); return pdPASS;}}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(esp_timer_get_time()<deadline);
    return pdFALSE;
}
BaseType_t xTaskCreate(void (*function)(void*),const char*,uint32_t,void* data,unsigned,TaskHandle_t* handle) {
    tasks.emplace_back([=]{try {function(data);} catch(const MockStop&) {}});
    if(handle) *handle=reinterpret_cast<void*>(tasks.size()); return pdPASS;
}
TickType_t xTaskGetTickCount() {return esp_timer_get_time()/1000;}
void vTaskDelay(TickType_t ms) {std::this_thread::sleep_for(std::chrono::milliseconds(ms)); if(stopping) throw MockStop{};}
void vTaskDelayUntil(TickType_t* wake,TickType_t duration) {*wake+=duration; int32_t remaining=*wake-xTaskGetTickCount(); vTaskDelay(remaining>0 ? remaining : 1);}
namespace mosicrokey::board {
bool CardMounted() {return mounted;}
void SetStorageFailed() {mounted=false;}
Result MountCard() {++mounts; mounted=true; return {};}
}
namespace mosicrokey::ui {
void Update(const Snapshot&) {}
void Frame(const std::vector<uint16_t>&) {}
}
int main() {
    std::string current="[{\"role\":\"assistant\",\"content\":\"fake publication\"}]";
    Check(CurrentTurnHistory("本轮发布 \"snake\"",current),"current publication request is materialized");
    auto* turn=cJSON_Parse(current.c_str());
    Check(cJSON_GetArraySize(turn)==1 && std::string(cJSON_GetObjectItem(cJSON_GetArrayItem(turn,0),"content")->valuestring)=="本轮发布 \"snake\"" && current.find("fake publication")==std::string::npos,"publication context excludes prior pseudo-tool claims and preserves current text");
    cJSON_Delete(turn);
    auto saved_current=current;
    Check(!CurrentTurnHistory(nullptr,current) && current==saved_current,"invalid current turn cannot silently replace context");
    auto claim=FinishAgent(true,AgentIntent::PublishGame,"","已成功调用game_preview发布新版本");
    Check(!claim.success && claim.text.find("未发布游戏")!=std::string::npos && claim.text.find("已成功")==std::string::npos,"model claim alone cannot complete game publication");
    auto ordinary=FinishAgent(true,AgentIntent::Conversation,"","hello");
    Check(ordinary.success && ordinary.text=="hello","ordinary conversation needs no game publication");
    auto published=FinishAgent(true,AgentIntent::PublishGame,"临时游戏已发布（重启后丢失）","完成");
    Check(published.success && published.text.find("重启后丢失")!=std::string::npos,"real preview publication retains its persistence limit");
    auto late_error=FinishAgent(false,AgentIntent::PublishGame,"TF 游戏版本已提交","超时");
    Check(!late_error.success && late_error.text.find("TF 游戏版本已提交")!=std::string::npos && late_error.text.find("超时")!=std::string::npos,"late model failure preserves actual TF publication and cause");
    auto auth_error=FinishAgent(false,AgentIntent::PublishGame,"","401");
    Check(!auth_error.success && auth_error.text=="401","upstream failure remains distinct from missing publication");
    std::string history="[]";
    Check(AppendRamHistory(history,"user",nullptr,"测试当前用户"),"text-only USER persistence is materialized");
    auto* records=cJSON_Parse(history.c_str());
    Check(cJSON_GetArraySize(records)==1 && std::string(cJSON_GetObjectItem(cJSON_GetArrayItem(records,0),"content")->valuestring)=="测试当前用户","history contains active user before core omits injection");
    cJSON_Delete(records);
    Check(AppendRamHistory(history,"assistant",nullptr,"回复"),"text-only final assistant persistence");
    for(int i=0;i<12;++i) Check(AppendRamHistory(history,"user",nullptr,("turn "+std::to_string(i)).c_str()),"bounded history append");
    records=cJSON_Parse(history.c_str()); Check(cJSON_GetArraySize(records)==8,"only newest eight records retained"); cJSON_Delete(records);
    Check(AppendRamHistory(history,"user",nullptr,std::string(15000,'a').c_str()),"large current user survives history trimming");
    auto previous=history;
    Check(!AppendRamHistory(history,"user",nullptr,std::string(17000,'a').c_str()) && history==previous,"oversized persistence fails without losing previous messages");
    const std::string secret="sk-$quoted(credential)";
    auto message=RedactSecret("Invalid key: "+secret+"; echoed again: "+secret,secret);
    Check(message.find(secret)==std::string::npos && message.find("Invalid key:")!=std::string::npos,"server credential echoes are redacted");
    Check(RedactSecret("ordinary response",secret)=="ordinary response","ordinary replies preserved");
    Check(RedactSecret("empty configuration","")=="empty configuration","empty secret is safe");
    App app; Voice voice; Agent agent(app); Games games;
    Check(bool(app.Start(voice,agent,games,games)),"start actual App with host scheduler");
    for(int i=0;i<100;++i) {
        Check(bool(app.Navigate(Mode::Xiaozhi)),"submit voice navigation");
        Wait([&]{return app.Read().mode==Mode::Xiaozhi && voice.foreground;},"voice owns audio");
        Check(bool(app.Navigate(Mode::Agent)),"submit agent navigation");
        Wait([&]{return app.Read().mode==Mode::Agent && !voice.foreground;},"release audio");
    }
    Check(bool(app.Ask("make a game",AgentIntent::PublishGame)),"agent submit");
    Check(agent.last_intent==AgentIntent::PublishGame,"creation intent reaches Agent through App"); app.Navigate(Mode::Home);
    Wait([&]{return app.Read().mode==Mode::Home && app.Read().agent_busy;},"request survives page change");
    app.Remount(); std::this_thread::sleep_for(std::chrono::milliseconds(80)); Check(mounts==0,"remount blocked during Agent request");
    app.Cancel(); app.Remount(); Wait([&]{return mounts==1;},"remount after cancellation");
    app.Navigate(Mode::Library); Wait([&]{return app.Read().mode==Mode::Library;},"library navigation");
    games.block=true; app.Input(Key::Confirm,true);
    Wait([&]{return app.Read().mode==Mode::Game && games.entered;},"launch selected game");
    app.Input(Key::Down,true); app.Input(Key::Down,false); Check(games.inputs==2,"game input focus");
    app.Navigate(Mode::Home); Wait([&]{return app.Read().mode==Mode::Home && !games.running;},"exit releases runtime");
    games.block=false; std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Check(app.Read().mode==Mode::Home,"late stopped frame cannot redirect navigation");
    app.Navigate(Mode::Library); Wait([&]{return app.Read().mode==Mode::Library;},"reenter library");
    app.Input(Key::Confirm,true); Wait([&]{return app.Read().mode==Mode::Game;},"relaunch game");
    games.fail=true; Wait([&]{return app.Read().mode==Mode::Library;},"script failure returns library");
    Check(app.Read().notice.find("script error")!=std::string::npos,"script error visible");
    games.list_error=true; app.Navigate(Mode::Library);
    Wait([&]{return app.Read().notice.find("Card read failed")!=std::string::npos;},"card read error visible");
    Check(!app.Read().storage && !mounted,"card read failure clears storage status");
    games.list_error=false; app.Remount();
    Wait([&]{return mounts==2 && app.Read().storage;},"remount recovers storage status");
    unsigned busy=0; for(int i=0;i<2000;++i) if(!app.Navigate(Mode::Home)) ++busy;
    Check(busy>0,"bounded operation queue reports busy");
    MockShutdown();
    std::cout<<"PASS "<<checks<<" checks: actual App actor, 100 audio focus switches, background request, remount guard, game exit race, queue saturation\n";
}
