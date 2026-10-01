// Compile the actual transport; mock only IDF HTTP and clock at its boundary.
#include "llm/claw_llm_http_transport.h"
#include "esp_http_client.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static esp_http_client_config_t config;
static unsigned checks, failures, calls, cleaned, timeout_updates;
static TickType_t ticks;
static unsigned pending_polls, cancel_at;
static int socket_timeout, initial_timeout;
static bool connected;
static bool cancel_during_connect;
static unsigned reconnect_at_call;
static esp_err_t pending_result;
static int pending_state;
static volatile bool cancelled;
const char* esp_err_to_name(esp_err_t error) {(void)error;return "MOCK_IDF_ERROR";}
static void check(bool condition,const char* name) {
    ++checks;
    if(!condition) {++failures; printf("FAIL %s\n",name);}
}
TickType_t xTaskGetTickCount(void) {return ticks;}
void vTaskDelay(TickType_t delay) {ticks+=delay; if(cancel_at && ticks>=cancel_at) cancelled=true;}
esp_err_t esp_crt_bundle_attach(void* unused) {(void)unused;return ESP_OK;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* value) {
    config=*value; initial_timeout=socket_timeout=value->timeout_ms; return &config;
}
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t h,int timeout) {
    (void)h; socket_timeout=timeout; ++timeout_updates; return ESP_OK;
}
esp_err_t esp_http_client_set_method(esp_http_client_handle_t h,int method) {(void)h;(void)method;return ESP_OK;}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t h,const char* n,const char* v) {(void)h;(void)n;(void)v;return ESP_OK;}
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t h,const char* text,int length) {
    (void)h;check(length==(int)strlen(text),"actual body length preserved");return ESP_OK;
}
esp_err_t esp_http_client_perform(esp_http_client_handle_t h) {
    ++calls;
    if(reconnect_at_call && calls==reconnect_at_call) {
        esp_http_client_event_t event={.event_id=HTTP_EVENT_DISCONNECTED,.user_data=config.user_data,.client=h};
        config.event_handler(&event);
        check(socket_timeout==initial_timeout,"disconnect restores TLS budget before redirect/reconnect");
        connected=false;
    }
    if(!connected) {
        connected=true;
        if(cancel_during_connect) cancelled=true;
        esp_http_client_event_t event={.event_id=HTTP_EVENT_ON_CONNECTED,.user_data=config.user_data,.client=h};
        config.event_handler(&event); // SDK ignores this callback's return value.
    }
    if(pending_polls) {
        --pending_polls; ticks+=socket_timeout;
        if(cancel_at && ticks>=cancel_at) cancelled=true;
        return pending_result;
    }
    char body[]="{\"ok\":true}";
    esp_http_client_event_t event={.event_id=HTTP_EVENT_ON_DATA,.user_data=config.user_data,.client=h,.data=body,.data_len=(int)strlen(body)};
    config.event_handler(&event);
    return ESP_OK;
}
int esp_http_client_get_status_code(esp_http_client_handle_t h) {(void)h;return 200;}
int esp_http_client_get_state(esp_http_client_handle_t h) {(void)h;return pending_state;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h) {(void)h;++cleaned;return ESP_OK;}
static esp_err_t run_with_error(const char* url,unsigned budget,unsigned polls,unsigned abort_at,bool abort_in_connect,unsigned reconnect,esp_err_t pending_error,int state) {
    ticks=calls=cleaned=timeout_updates=0; connected=cancelled=false;
    pending_polls=polls; cancel_at=abort_at;
    cancel_during_connect=abort_in_connect;
    reconnect_at_call=reconnect;
    pending_result=pending_error; pending_state=state;
    claw_llm_http_json_request_t request={.url=url,.body="{\"model\":\"test\"}",.api_key="test-only",.auth_type="bearer",.timeout_ms=budget,.abort_flag=&cancelled};
    claw_llm_http_response_t response={0}; char* error=NULL;
    esp_err_t result=claw_llm_http_post_json(&request,&response,&error);
    check(cleaned==1,"client cleaned exactly once");
    check(result==ESP_OK ? response.body && strcmp(response.body,"{\"ok\":true}")==0 && !error : error!=NULL,"actual success/error result retained");
    claw_llm_http_response_free(&response);free(error);return result;
}
static esp_err_t run(const char* url,unsigned budget,unsigned polls,unsigned abort_at,bool abort_in_connect,unsigned reconnect) {
    return run_with_error(url,budget,polls,abort_at,abort_in_connect,reconnect,ESP_ERR_HTTP_EAGAIN,HTTP_STATE_REQ_COMPLETE_HEADER);
}
int main(void) {
    check(run("https://test.invalid",60000,40,0,false,0)==ESP_OK,"slow response succeeds after many short I/O polls");
    check(initial_timeout==60000 && socket_timeout==100 && timeout_updates==1,"TLS retains connection budget; connected I/O uses 100ms");
    check(config.is_async && !config.keep_alive_enable && calls==41 && ticks==4400,"same async handle progresses without replay or pool reuse");
    check(run("https://test.invalid",60000,1000,25,false,0)==ESP_ERR_INVALID_STATE && calls==1 && ticks==110,"cancel interrupts a stalled connected socket poll");
    check(run("https://test.invalid",350,100,0,false,0)==ESP_ERR_TIMEOUT && calls==4 && ticks==440,"outer deadline terminates repeated pending I/O");
    check(run("https://test.invalid",50,100,1,false,0)==ESP_ERR_INVALID_STATE && initial_timeout==50 && socket_timeout==50 && ticks==60,"poll never exceeds a smaller configured budget");
    check(run("http://test.invalid",700,0,0,false,0)==ESP_OK && !config.is_async && initial_timeout==700 && socket_timeout==700 && timeout_updates==0,"plain HTTP connection behavior preserved");
    check(run("https://test.invalid",60000,1000,0,true,0)==ESP_ERR_INVALID_STATE &&
          socket_timeout==100 && timeout_updates==1 && calls==1 && ticks==110,
          "cancel during TLS still shortens I/O when SDK ignores connected callback return");
    check(run("https://test.invalid",60000,6,0,false,3)==ESP_OK &&
          timeout_updates==3 && socket_timeout==100 && calls==7 && ticks==660,
          "reconnect reapplies short I/O polling without resetting request deadline");
    check(run("https://test.invalid",350,100,0,false,3)==ESP_ERR_TIMEOUT &&
          calls==4 && ticks==440 && timeout_updates==3,
          "reconnect cannot restart a nearly exhausted outer deadline");
    check(run_with_error("https://test.invalid",60000,6,0,false,0,ESP_ERR_HTTP_WRITE_DATA,HTTP_STATE_REQ_COMPLETE_HEADER)==ESP_OK &&
          calls==7 && ticks==660,"partial async POST progresses on the same body when errno is stale");
    check(run_with_error("https://test.invalid",60000,6,0,false,0,ESP_ERR_HTTP_WRITE_DATA,HTTP_STATE_INIT)!=ESP_OK &&
          calls==1,"closed/header write errors are not replayed as partial POST");
    check(run_with_error("https://test.invalid",60000,100,25,false,0,ESP_ERR_HTTP_WRITE_DATA,HTTP_STATE_REQ_COMPLETE_HEADER)==ESP_ERR_INVALID_STATE &&
          calls==1 && ticks==110,"cancel still interrupts normalized partial POST without replay");
    printf("%s %u actual transport checks; failures=%u\n",failures?"FAIL":"PASS",checks,failures);
    return failures?1:0;
}
