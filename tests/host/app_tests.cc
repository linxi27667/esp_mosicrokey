#include "mosicrokey/app.hpp"
#include "mosicrokey/hal_storage.hpp"
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
#include <algorithm>
using namespace mosicrokey;
namespace {
std::atomic<bool> stopping{false};
struct Queue {unsigned capacity,size; std::mutex mutex; std::deque<std::vector<uint8_t>> items;};
std::vector<std::unique_ptr<Queue>> queues;
struct MockTask {std::thread thread; std::atomic<bool> deleted{false}; std::atomic<unsigned> notifications{0};};
std::vector<std::unique_ptr<MockTask>> tasks;
thread_local MockTask* current_task=nullptr;
unsigned task_failure_countdown=0;
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
    std::atomic<bool> list_block{false}, list_entered{false};
    std::atomic<bool> tool_block{false}, tool_entered{false};
    std::atomic<unsigned> version{1};
    Result List(std::vector<GameInfo>& list) override {
        if(list_error) return Result::Fail(Error::Storage,"Card read failed");
        auto revision=std::to_string(version.load());
        list_entered=true;
        while(list_block && !stopping) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        list={{"snake","贪吃蛇",revision,"virtual"}}; return {};
    }
    Result Tool(const std::string&,const std::string&,std::string& output) override {
        tool_entered=true;
        while(tool_block && !stopping) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        output="{}"; return {};
    }
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
void MockShutdown() {stopping=true; for(auto& task:tasks) if(task->thread.joinable()) task->thread.join(); tasks.clear(); queues.clear(); stopping=false;}
void vQueueDelete(QueueHandle_t handle) {std::erase_if(queues,[&](const auto& queue){return queue.get()==handle;});}
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
        if(stopping || (current_task && current_task->deleted)) throw MockStop{};
        {std::lock_guard lock(queue.mutex); if(!queue.items.empty()) {std::memcpy(item,queue.items.front().data(),queue.size); queue.items.pop_front(); return pdPASS;}}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(esp_timer_get_time()<deadline);
    return pdFALSE;
}
BaseType_t xTaskCreate(void (*function)(void*),const char*,uint32_t,void* data,unsigned,TaskHandle_t* handle) {
    if(task_failure_countdown && --task_failure_countdown==0) return pdFALSE;
    auto* task=tasks.emplace_back(std::make_unique<MockTask>()).get();
    task->thread=std::thread([=]{current_task=task; try {function(data);} catch(const MockStop&) {}});
    if(handle) *handle=task; return pdPASS;
}
void vTaskDelete(TaskHandle_t handle) {
    auto* task=static_cast<MockTask*>(handle); task->deleted=true;
    if(task->thread.joinable()) task->thread.join();
}
void xTaskNotifyGive(TaskHandle_t handle) {++static_cast<MockTask*>(handle)->notifications;}
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t timeout) {
    auto deadline=esp_timer_get_time()+int64_t(timeout)*1000;
    while(!current_task->notifications) {
        if(stopping || current_task->deleted) throw MockStop{};
        if(esp_timer_get_time()>=deadline) return 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return clear ? current_task->notifications.exchange(0) : current_task->notifications.fetch_sub(1);
}
TickType_t xTaskGetTickCount() {return esp_timer_get_time()/1000;}
void vTaskDelay(TickType_t ms) {std::this_thread::sleep_for(std::chrono::milliseconds(ms)); if(stopping || (current_task && current_task->deleted)) throw MockStop{};}
void vTaskDelayUntil(TickType_t* wake,TickType_t duration) {*wake+=duration; int32_t remaining=*wake-xTaskGetTickCount(); vTaskDelay(remaining>0 ? remaining : 1);}
namespace mosicrokey::hal {
bool Hal_Storage_Is_Mounted() {return mounted;}
void Hal_Storage_Mark_Failed() {mounted=false;}
esp_err_t Hal_Storage_Mount() {++mounts; mounted=true; return ESP_OK;}
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
    {
        App failed; Voice voice; Agent agent(failed); Games games;
        task_failure_countdown=1;
        Check(!failed.System_Start(voice,agent,games,games),"first worker allocation failure reported");
        Check(queues.empty() && tasks.empty(),"first worker failure leaves no queue or task");
        task_failure_countdown=2;
        Check(!failed.System_Start(voice,agent,games,games),"second worker allocation failure reported");
        Check(queues.empty() && std::all_of(tasks.begin(),tasks.end(),[](const auto& task){return task->deleted.load();}),"partial startup deletes gated worker and queue");
        Check(bool(failed.System_Start(voice,agent,games,games)),"same App retries successfully after both failure points");
        Check(bool(failed.Page_Navigate(Mode::Library)),"retry accepts commands");
        Wait([&]{return failed.State_Read().mode==Mode::Library;},"retried workers process actual navigation");
        MockShutdown();
    }
    App app; Voice voice; Agent agent(app); Games games;
    Check(bool(app.System_Start(voice,agent,games,games)),"start actual App with host scheduler");
    for(int i=0;i<100;++i) {
        Check(bool(app.Page_Navigate(Mode::Xiaozhi)),"submit voice navigation");
        Wait([&]{return app.State_Read().mode==Mode::Xiaozhi && voice.foreground;},"voice owns audio");
        Check(bool(app.Page_Navigate(Mode::Agent)),"submit agent navigation");
        Wait([&]{return app.State_Read().mode==Mode::Agent && !voice.foreground;},"release audio");
    }
    Check(bool(app.Agent_Ask("make a game",AgentIntent::PublishGame)),"agent submit");
    Check(agent.last_intent==AgentIntent::PublishGame,"creation intent reaches Agent through App"); app.Page_Navigate(Mode::Home);
    Wait([&]{return app.State_Read().mode==Mode::Home && app.State_Read().agent_busy;},"request survives page change");
    app.Storage_Remount(); std::this_thread::sleep_for(std::chrono::milliseconds(80)); Check(mounts==0,"remount blocked during Agent request");
    app.Agent_Cancel(); app.Storage_Remount(); Wait([&]{return mounts==1;},"remount after cancellation");
    app.Page_Navigate(Mode::Library); Wait([&]{return app.State_Read().mode==Mode::Library;},"library navigation");
    games.block=true; app.Key_Input(Key::Confirm,true);
    Wait([&]{return app.State_Read().mode==Mode::Game && games.entered;},"launch selected game");
    app.Key_Input(Key::Down,true); app.Key_Input(Key::Down,false); Check(games.inputs==2,"game input focus");
    app.Page_Navigate(Mode::Home); Wait([&]{return app.State_Read().mode==Mode::Home && !games.running;},"exit releases runtime");
    games.block=false; std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Check(app.State_Read().mode==Mode::Home,"late stopped frame cannot redirect navigation");
    app.Page_Navigate(Mode::Library); Wait([&]{return app.State_Read().mode==Mode::Library;},"reenter library");
    app.Key_Input(Key::Confirm,true); Wait([&]{return app.State_Read().mode==Mode::Game;},"relaunch game");
    games.fail=true; Wait([&]{return app.State_Read().mode==Mode::Library;},"script failure returns library");
    Check(app.State_Read().notice.find("script error")!=std::string::npos,"script error visible");
    games.list_error=true; app.Page_Navigate(Mode::Library);
    Wait([&]{return app.State_Read().notice.find("Card read failed")!=std::string::npos;},"card read error visible");
    Check(!app.State_Read().storage && !mounted,"card read failure clears storage status");
    games.list_error=false; app.Storage_Remount();
    Wait([&]{return mounts==2 && app.State_Read().storage;},"remount recovers storage status");
    games.tool_entered=false; games.tool_block=true;
    std::thread source_reader([&]{std::string output; app.Game_Read_Source("snake",0,output);});
    Wait([&]{return games.tool_entered.load();},"hold source file read");
    Check(bool(app.Storage_Remount()),"remount request queued during source read");
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    Check(mounts==2,"TF stays mounted while source read owns storage operation lock");
    games.tool_block=false; source_reader.join();
    Wait([&]{return mounts==3;},"TF remount resumes after source read finishes");
    unsigned busy=0; for(int i=0;i<2000;++i) if(!app.Page_Navigate(Mode::Home)) ++busy;
    Check(busy>0,"bounded operation queue reports busy");
    Wait([&]{return app.State_Read().mode==Mode::Home;},"drain navigation before refresh saturation probe");
    games.list_entered=false; games.list_block=true;
    while(!app.Page_Navigate(Mode::Library)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Wait([&]{return games.list_entered.load();},"hold actual library operation while filling command queue");
    for(unsigned i=0;i<12;++i) Check(bool(app.Page_Navigate(Mode::Home)),"fill every bounded operation slot");
    Check(!app.Page_Navigate(Mode::Home),"queue saturated during completion");
    games.version=2; app.Agent(999,false,"new game published"); games.list_block=false;
    Wait([&]{auto snapshot=app.State_Read(); return !snapshot.games.empty() && snapshot.games[0].revision=="2";},"Agent completion refresh survives full command queue");
    MockShutdown();
    std::cout<<"PASS "<<checks<<" checks: actual App actor, 100 audio focus switches, background request, remount guard, game exit race, queue saturation\n";
}
