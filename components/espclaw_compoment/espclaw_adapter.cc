#include "mosicrokey/espclaw.hpp"
#include "mosicrokey/board.hpp"
#include "redaction.hpp"
#include "ram_history.hpp"
#include "completion.hpp"
#include "claw_core.h"
#include "claw_cap.h"
#include "claw_memory.h"
#include "claw_skill.h"
#include "claw_paths.h"
#include <cJSON.h>
#include <nvs.h>
#include <esp_log.h>
#include <freertos/task.h>
#include <atomic>
#include <mutex>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>

extern const char skill_text[] asm("_binary_SKILL_md_start");
extern const char snake_text[] asm("_binary_main_lua_start");

namespace mosicrokey {
namespace {
Result EspResult(esp_err_t error) {
    return error == ESP_OK ? Result{} : Result::Fail(Error::Runtime, esp_err_to_name(error));
}
std::string NvsString(nvs_handle_t handle, const char* name) {
    size_t length = 0;
    if (nvs_get_str(handle, name, nullptr, &length) != ESP_OK || length > 2048) return {};
    std::string result(length, '\0');
    if (nvs_get_str(handle, name, result.data(), &length) != ESP_OK) return {};
    result.resize(length ? length - 1 : 0); return result;
}
class AgentAdapter;
AgentAdapter* active = nullptr; // One product instance, required by the C capability callbacks.
class AgentAdapter final : public IAgentService {
public:
    AgentAdapter(IGameLibrary& library, IEvents& events) : library_(library), events_(events) {
        // A compatible server may echo credentials in its error body. Publish only redacted results.
        esp_log_level_set("llm_http",ESP_LOG_NONE);
        // The core logs raw model replies, tool arguments and HTTP error bodies.
        // Product progress below retains IDs and tool status without exposing them.
        esp_log_level_set("claw_core",ESP_LOG_NONE);
        active = this;
        claw_paths_set(CLAW_PATH_DATA, "/sdcard/agent");
        claw_paths_set(CLAW_PATH_SYSTEM, "/system");
        registration_ = claw_cap_init();
        if (registration_ == ESP_OK) registration_ = RegisterTools();
        nvs_handle_t handle;
        if (nvs_open("mosi_agent", NVS_READONLY, &handle) == ESP_OK) {
            config_ = {NvsString(handle,"url"), NvsString(handle,"model"), NvsString(handle,"key")};
            nvs_close(handle);
        }
        if (!config_.url.empty()) Initialize();
    }
    Result Configure(const AgentConfig& config) override {
        std::lock_guard lock(control_);
        if (busy_) return Result::Fail(Error::Busy, "Agent busy; cancel first");
        if ((!config.url.starts_with("https://") && !config.url.starts_with("http://")) ||
            config.url.size() > 512 || config.model.empty() || config.model.size() > 128 ||
            config.key.empty() || config.key.size() > 1024) return Result::Fail(Error::Invalid,"Invalid URL, model or key");
        nvs_handle_t handle;
        auto error = nvs_open("mosi_agent", NVS_READWRITE, &handle);
        if (error != ESP_OK) return EspResult(error);
        error = nvs_set_str(handle,"url",config.url.c_str());
        if (error == ESP_OK) error = nvs_set_str(handle,"model",config.model.c_str());
        if (error == ESP_OK) error = nvs_set_str(handle,"key",config.key.c_str());
        if (error == ESP_OK) error = nvs_commit(handle);
        nvs_close(handle);
        if (error != ESP_OK) return EspResult(error);
        config_ = config;
        return Initialize();
    }
    Result Submit(const std::string& text, uint32_t& request,AgentIntent intent) override {
        std::lock_guard lock(control_);
        if (!core_) return Result::Fail(Error::NotReady, "请先用 agent_config 设置模型地址、模型名和密钥");
        if (text.empty() || text.size() > 4096) return Result::Fail(Error::Invalid,"Request length 1..4096 bytes");
        if (busy_.exchange(true)) return Result::Fail(Error::Busy,"Agent busy");
        request = ++sequence_; current_ = request; repairs_ = 0;
        publication_.clear();
        intent_=intent;
        std::string requested=text;
        if(intent==AgentIntent::PublishGame)
            requested="Publish this game through actual native tool_calls. A text claim or source code is not publication. Only game_preview/game_commit success fulfills this request. "+text;
        claw_core_request_t item{};
        item.request_id = request; item.session_id = "mosicrokey"; item.user_text = requested.c_str();
        item.source_channel = "serial"; item.source_chat_id = "device";
        events_.Agent(request,true,"正在请求模型");
        auto error = claw_core_submit(core_, &item, 0);
        if (error != ESP_OK) { busy_ = false; events_.Agent(request,false,esp_err_to_name(error)); return EspResult(error); }
        return {};
    }
    Result Cancel(uint32_t request) override {
        std::lock_guard lock(control_);
        if (!core_ || !busy_ || request != current_) return Result::Fail(Error::NotReady,"No matching request");
        auto result=EspResult(claw_core_cancel_request(core_, request));
        if(result) events_.Agent(request,true,"正在取消");
        return result;
    }
    esp_err_t Tool(const char* name, const char* input, const claw_cap_call_context_t* context, char* output, size_t capacity) {
        if (!context || context->request_id != current_) return ESP_ERR_INVALID_STATE;
        std::string result;
        Result status;
        if (repairs_ > 3 && std::strcmp(name,"game_list") != 0) status = Result::Fail(Error::Invalid,"Three correction attempts exhausted");
        else status = library_.Tool(name,input ? input : "{}",result);
        if (!status) {
            if (std::strcmp(name,"game_validate") == 0 || std::strcmp(name,"game_preview") == 0) ++repairs_;
            cJSON* error = cJSON_CreateObject();
            cJSON_AddStringToObject(error,"error",status.message.c_str());
            cJSON_AddNumberToObject(error,"code",static_cast<int>(status.error));
            char* json = cJSON_PrintUnformatted(error); result = json ? json : "{\"error\":\"allocation failed\"}";
            cJSON_free(json); cJSON_Delete(error);
            if (status.error == Error::Storage) board::SetStorageFailed();
        }
        if(status && (std::strcmp(name,"game_preview")==0 || std::strcmp(name,"game_commit")==0)) {
            std::lock_guard lock(control_);
            publication_=std::strcmp(name,"game_preview")==0 ? "临时游戏已发布（重启后丢失）" : "TF 游戏版本已提交";
        }
        if (result.size() >= capacity) return ESP_ERR_INVALID_SIZE;
        std::memcpy(output,result.c_str(),result.size()+1);
        events_.Agent(context->request_id,true,std::string(name) + ": " + (status ? "完成" : status.message));
        ESP_LOGI("mosi_agent","request=%lu tool=%s result=%d",static_cast<unsigned long>(context->request_id),name,static_cast<int>(status.error));
        if(!status) ESP_LOGW("mosi_agent","tool_error=%.160s",RedactSecret(status.message,config_.key).c_str());
        return ESP_OK; // Tool errors are structured observations for the real repair loop.
    }
private:
    template<unsigned Index> static esp_err_t Execute(const char* input, const claw_cap_call_context_t* context, char* output, size_t capacity) {
        static constexpr const char* names[] = {"game_begin","game_write_file","game_validate","game_commit","game_list","game_preview"};
        return active ? active->Tool(names[Index],input,context,output,capacity) : ESP_ERR_INVALID_STATE;
    }
    esp_err_t RegisterTools() {
        static const char begin[] = R"({"type":"object","properties":{"id":{"type":"string"},"title":{"type":"string"}},"required":["id","title"],"additionalProperties":false})";
        static const char write[] = R"({"type":"object","properties":{"job":{"type":"string"},"path":{"type":"string"},"content":{"type":"string"},"encoding":{"type":"string","enum":["utf8","base64"]}},"required":["job","path","content"],"additionalProperties":false})";
        static const char job[] = R"({"type":"object","properties":{"job":{"type":"string"}},"required":["job"],"additionalProperties":false})";
        static const claw_cap_descriptor_t descriptors[] = {
            {"game_begin","game_begin","game","Stage a new game revision",CLAW_CAP_KIND_CALLABLE,CLAW_CAP_FLAG_CALLABLE_BY_LLM,begin,nullptr,nullptr,nullptr,Execute<0>},
            {"game_write_file","game_write_file","game","Write Lua or RGB565 asset",CLAW_CAP_KIND_CALLABLE,CLAW_CAP_FLAG_CALLABLE_BY_LLM,write,nullptr,nullptr,nullptr,Execute<1>},
            {"game_validate","game_validate","game","Validate and probe sandbox",CLAW_CAP_KIND_CALLABLE,CLAW_CAP_FLAG_CALLABLE_BY_LLM,job,nullptr,nullptr,nullptr,Execute<2>},
            {"game_commit","game_commit","game","Atomically install validated revision",CLAW_CAP_KIND_CALLABLE,CLAW_CAP_FLAG_CALLABLE_BY_LLM,job,nullptr,nullptr,nullptr,Execute<3>},
            {"game_list","game_list","game","List games, or read manifest and 4096-byte Lua source pages by id and offset",CLAW_CAP_KIND_CALLABLE,CLAW_CAP_FLAG_CALLABLE_BY_LLM,R"({"type":"object","properties":{"id":{"type":"string"},"offset":{"type":"integer","minimum":0}},"additionalProperties":false})",nullptr,nullptr,nullptr,Execute<4>},
            {"game_preview","game_preview","game","Validate and publish temporary RAM-only Lua preview for USB testing; lost on reboot, never installed to TF",CLAW_CAP_KIND_CALLABLE,CLAW_CAP_FLAG_CALLABLE_BY_LLM,R"({"type":"object","properties":{"id":{"type":"string"},"title":{"type":"string"},"source":{"type":"string"}},"required":["id","title","source"],"additionalProperties":false})",nullptr,nullptr,nullptr,Execute<5>}};
        static const claw_cap_group_t group{"game","MosiCrokey games","1",descriptors,6,nullptr,nullptr,nullptr,nullptr};
        auto error = claw_cap_register_group(&group);
        if (error == ESP_OK) error = claw_cap_start_all();
        const char* visible[] = {"game"};
        if (error == ESP_OK) error = claw_cap_set_llm_visible_groups(visible,1);
        return error;
    }
    static esp_err_t Instructions(const claw_core_request_t*, claw_core_context_t* context, void*) {
        std::string text=skill_text;
        // Metadata describes skill management, not whether game tools may write.
        auto header_end=text.find("\n---",3);
        if(text.starts_with("---") && header_end!=std::string::npos) text.erase(0,header_end+4);
        text += std::string("\nCompatible snake source example (not a tool invocation):\n```lua\n")+snake_text+"\n```";
        context->kind = CLAW_CORE_CONTEXT_KIND_SYSTEM_PROMPT;
        context->content = strdup(text.c_str()); return context->content ? ESP_OK : ESP_ERR_NO_MEM;
    }
    void PrepareStorage() {
        if (memory_ready_ || !board::CardMounted()) return;
        char sessions[128], memory[128], skills[128];
        claw_paths_join(CLAW_PATH_DATA,"sessions",sessions,sizeof(sessions));
        claw_paths_join(CLAW_PATH_DATA,"memory",memory,sizeof(memory));
        claw_paths_join(CLAW_PATH_DATA,"skill_state",skills,sizeof(skills));
        claw_memory_config_t settings{}; settings.session_root_dir=sessions; settings.memory_root_dir=memory;
        settings.max_message_chars=8192; settings.max_tool_iterations=16; settings.llm.backend_type="openai_compatible";
        memory_ready_ = claw_memory_init(&settings) == ESP_OK;
        claw_skill_config_t skill{skills,16384};
        if (claw_skill_init(&skill) == ESP_OK && claw_skill_add_directory("/system/skills") == ESP_OK)
            claw_skill_activate_for_session("mosicrokey","game-creator");
    }
    static esp_err_t StartRequest(const claw_core_request_t*, void* user) {
        auto& self = *static_cast<AgentAdapter*>(user); self.PrepareStorage(); return ESP_OK;
    }
    static esp_err_t History(const claw_core_request_t* request, claw_core_context_t* context, void* user) {
        auto& self = *static_cast<AgentAdapter*>(user);
        if(self.intent_==AgentIntent::PublishGame) {
            std::string current;
            if(!CurrentTurnHistory(request->user_text,current)) return ESP_ERR_NO_MEM;
            context->kind=CLAW_CORE_CONTEXT_KIND_MESSAGES;
            context->content=strdup(current.c_str()); return context->content ? ESP_OK : ESP_ERR_NO_MEM;
        }
        if (self.memory_ready_ && board::CardMounted()) {
            auto error=claw_memory_session_history_provider.collect(request,context,claw_memory_session_history_provider.user_ctx);
            if(error==ESP_OK) return ESP_OK;
            if(error!=ESP_ERR_NOT_FOUND && error!=ESP_ERR_NO_MEM) {
                board::SetStorageFailed(); self.events_.Notice("TF 会话读取失败；已切换临时会话");
            }
        }
        context->kind = CLAW_CORE_CONTEXT_KIND_MESSAGES;
        context->content = strdup(self.history_.c_str()); return context->content ? ESP_OK : ESP_ERR_NO_MEM;
    }
    static esp_err_t Persist(const claw_core_context_persist_batch_t* batch, void* user) {
        auto& self = *static_cast<AgentAdapter*>(user);
        for (size_t i=0; i<batch->record_count; ++i) {
            if (batch->records[i].type != CLAW_CORE_CONTEXT_RECORD_USER && batch->records[i].type != CLAW_CORE_CONTEXT_RECORD_ASSISTANT_FINAL) continue;
            auto& record=batch->records[i];
            if(!AppendRamHistory(self.history_,record.type==CLAW_CORE_CONTEXT_RECORD_USER ? "user" : "assistant",record.message_json,record.text))
                return ESP_ERR_NO_MEM;
        }
        if (self.memory_ready_ && board::CardMounted()) {
            if (claw_memory_persist_context_callback(batch,nullptr) != ESP_OK) {
                board::SetStorageFailed(); self.events_.Notice("TF 会话保存失败；已切换临时会话");
            }
        }
        return ESP_OK;
    }
    Result Initialize() {
        if (registration_ != ESP_OK) return EspResult(registration_);
        claw_core_config_t settings{};
        settings.api_key=config_.key.c_str(); settings.backend_type="openai_compatible";
        settings.model=config_.model.c_str(); settings.base_url=config_.url.c_str(); settings.auth_type="bearer";
        settings.max_tokens_field="max_tokens"; settings.timeout_ms=60000; settings.max_tokens=8192; settings.supports_tools=true;
        settings.system_prompt="You are ESPClaw running on real MosiCrokey hardware. Creating or modifying games requires actual calls to the registered external API tools. Do not print tool invocations as text or call them inside Lua. Only report success after a successful tool result. Ordinary conversation needs no tools.";
        settings.persist_context=Persist; settings.persist_context_user_ctx=this;
        settings.on_request_start=StartRequest; settings.on_request_start_user_ctx=this;
        settings.call_cap=claw_cap_call_from_core;
        settings.task_stack_size=16384; settings.task_priority=1; settings.task_core=0;
        settings.max_tool_iterations=16; settings.request_queue_len=2; settings.response_queue_len=2; settings.max_context_providers=8;
        if (core_) return EspResult(claw_core_update_llm_config(core_,&settings));
        auto error = claw_core_create(&settings,&core_); if (error != ESP_OK) return EspResult(error);
        claw_core_context_provider_t instructions{"game-creator",Instructions,this,CLAW_CORE_CONTEXT_PROVIDER_FLAG_REQUEST_START_ONLY};
        claw_core_context_provider_t history{"mosicrokey-history",History,this,CLAW_CORE_CONTEXT_PROVIDER_FLAG_REQUEST_START_ONLY};
        error=claw_core_add_context_provider(core_,&instructions);
        if (error==ESP_OK) error=claw_core_add_context_provider(core_,&history);
        if (error==ESP_OK) error=claw_core_add_context_provider(core_,&claw_cap_root_agent_tools_provider);
        if (error==ESP_OK) error=claw_core_start(core_);
        if (error==ESP_OK && xTaskCreate(Receive,"mosi_agent_rx",6144,this,1,nullptr)!=pdPASS) error=ESP_ERR_NO_MEM;
        if (error!=ESP_OK) { claw_core_stop(core_,2000); claw_core_destroy(core_); core_=nullptr; }
        return EspResult(error);
    }
    static void Receive(void* user) {
        auto& self = *static_cast<AgentAdapter*>(user);
        for (;;) {
            claw_core_response_t response{};
            if (claw_core_receive(self.core_,&response,1000)!=ESP_OK) continue;
            self.library_.DiscardStaging();
            AgentCompletion completion;
            {
                std::lock_guard lock(self.control_);
                auto text=RedactSecret(response.status==CLAW_CORE_RESPONSE_STATUS_OK ?
                    (response.text ? response.text : "完成") : (response.error_message ? response.error_message : "Agent 请求失败"),self.config_.key);
                completion=FinishAgent(response.status==CLAW_CORE_RESPONSE_STATUS_OK,self.intent_,self.publication_,text);
            }
            self.events_.Agent(response.request_id,false,completion.text);
            ESP_LOGI("mosi_agent","request=%lu completed status=%d backend_status=%d",static_cast<unsigned long>(response.request_id),completion.success ? 0 : 1,static_cast<int>(response.status));
            claw_core_response_free(&response);
            self.busy_=false;
        }
    }
    IGameLibrary& library_; IEvents& events_;
    AgentConfig config_; claw_core_handle_t core_=nullptr;
    esp_err_t registration_=ESP_OK;
    std::mutex control_;
    std::atomic<bool> busy_{false}; std::atomic<uint32_t> current_{0};
    uint32_t sequence_=0; unsigned repairs_=0;
    bool memory_ready_=false; std::string history_="[]";
    std::string publication_;
    AgentIntent intent_=AgentIntent::Conversation;
};
}
std::unique_ptr<IAgentService> CreateAgent(IGameLibrary& library,IEvents& events) {
    return std::make_unique<AgentAdapter>(library,events);
}
} // namespace mosicrokey
