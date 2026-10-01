// Exercise the real pinned OpenAI backend with simulated HTTP outcomes and time.
#include "llm/backends/claw_llm_backend_openai_compatible.h"
#include "llm/claw_llm_http_transport.h"
#include "llm/media/claw_media_pipeline.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks, failures, calls;
static uint32_t ticks, cancel_at, request_time, last_timeout;
static int outcomes[3];
static volatile bool cancelled;
static void check(bool ok, const char* name) {
    ++checks; if (!ok) { ++failures; printf("FAIL %s\n",name); }
}
TickType_t xTaskGetTickCount(void) { return ticks; }
void vTaskDelay(TickType_t delay) { ticks+=delay; if(cancel_at && ticks>=cancel_at) cancelled=true; }
esp_err_t claw_llm_http_post_json(const claw_llm_http_json_request_t* request, claw_llm_http_response_t* response, char** error) {
    cJSON* body=cJSON_Parse(request->body);
    const cJSON* choice=cJSON_GetObjectItem(body,"tool_choice");
    check(cJSON_IsString(choice) && strcmp(choice->valuestring,"auto")==0,"native tool choice survives every attempt");
    cJSON_Delete(body);
    last_timeout=request->timeout_ms;
    ticks+=request_time;
    response->status_code=outcomes[calls++];
    if(response->status_code==200) {response->body=strdup("{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"ok\"}}]}"); return ESP_OK;}
    *error=strdup("simulated HTTP error"); return ESP_FAIL;
}
void claw_llm_http_response_free(claw_llm_http_response_t* response) {free(response->body); memset(response,0,sizeof(*response));}
void claw_llm_response_free(claw_llm_response_t* response) {
    free(response->text); free(response->reasoning_content); free(response->raw_message_json);
    for(size_t i=0;i<response->tool_call_count;++i) {free(response->tool_calls[i].id);free(response->tool_calls[i].name);free(response->tool_calls[i].arguments_json);}
    free(response->tool_calls); memset(response,0,sizeof(*response));
}
esp_err_t claw_media_prepare_asset(const claw_media_asset_t* a,const claw_llm_model_profile_t* b,size_t c,claw_media_prepared_t* d,char** e) {(void)a;(void)b;(void)c;(void)d;(void)e;return ESP_ERR_NOT_SUPPORTED;}
void claw_media_prepared_free(claw_media_prepared_t* value) {free(value->payload);}
static esp_err_t run(int first,int second,int third,unsigned budget,unsigned cancel,unsigned duration) {
    ticks=calls=0; cancelled=false; cancel_at=cancel; request_time=duration;
    outcomes[0]=first;outcomes[1]=second;outcomes[2]=third;
    const claw_llm_backend_vtable_t* backend=claw_llm_backend_openai_compatible_vtable();
    claw_llm_runtime_config_t config={.api_key="test-only",.model="test-model",.base_url="https://test.invalid",.timeout_ms=budget,.max_tokens=128};
    claw_llm_model_profile_t profile={.chat_path="/chat/completions",.max_tokens_field="max_tokens",.supports_tools=true};
    void* context=NULL;char* error=NULL;
    check(backend->init(&config,&profile,&context,&error)==ESP_OK,"backend initializes");
    cJSON* messages=cJSON_Parse("[{\"role\":\"user\",\"content\":\"test\"}]");
    claw_llm_chat_request_t request={.system_prompt="test",.messages=messages,.tools_json="[]",.abort_flag=&cancelled};
    claw_llm_response_t response={0};
    esp_err_t result=backend->chat(context,&profile,&request,&response,&error);
    check(result!=ESP_OK || (response.text && strcmp(response.text,"ok")==0 && !error),"success parses actual response and clears prior error");
    check(result==ESP_OK || error!=NULL,"failure retains an error");
    claw_llm_response_free(&response);free(error);cJSON_Delete(messages);backend->deinit(context);
    return result;
}
int main(void) {
    check(run(429,200,200,60000,0,100)==ESP_OK && calls==2 && ticks==4200 && last_timeout==55900,"429 recovers after four seconds within original budget");
    check(run(503,429,200,60000,0,100)==ESP_OK && calls==3 && ticks==12300 && last_timeout==47800,"two overload responses recover after bounded backoff");
    check(run(429,429,429,60000,0,100)!=ESP_OK && calls==3,"persistent overload stops after two retries");
    check(run(401,200,200,60000,0,100)!=ESP_OK && calls==1 && ticks==100,"authentication error is never retried");
    check(run(0,200,200,60000,0,100)!=ESP_OK && calls==1,"ambiguous network failure is never retried");
    check(run(429,200,200,60000,200,100)==ESP_ERR_INVALID_STATE && calls==1 && ticks==200,"cancel interrupts backoff without another HTTP request");
    check(run(429,200,200,1000,0,100)==ESP_ERR_TIMEOUT && calls==1 && ticks==1000,"backoff respects original deadline");
    check(run(200,200,200,60000,0,100)==ESP_OK && calls==1 && ticks==100,"ordinary success has no backoff");
    check(run(200,200,200,60000,0,61000)==ESP_ERR_TIMEOUT && calls==1,"late HTTP 200 is discarded rather than executing tools after deadline");
    printf("%s %u backend retry checks; failures=%u\n",failures?"FAIL":"PASS",checks,failures);
    return failures?1:0;
}
