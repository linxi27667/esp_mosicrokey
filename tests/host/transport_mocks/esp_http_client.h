#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#define ESP_ERR_HTTP_EAGAIN 0x7007
#define ESP_ERR_HTTP_WRITE_DATA 0x7003
#define HTTP_STATE_INIT 0
#define HTTP_STATE_REQ_COMPLETE_HEADER 3
#define HTTP_METHOD_POST 1
#define HTTP_EVENT_ON_CONNECTED 1
#define HTTP_EVENT_ON_DATA 2
#define HTTP_EVENT_DISCONNECTED 3
typedef void* esp_http_client_handle_t;
typedef struct {
    int event_id;
    void* user_data;
    void* data;
    int data_len;
    esp_http_client_handle_t client;
} esp_http_client_event_t;
typedef struct {
    const char* url;
    esp_err_t (*event_handler)(esp_http_client_event_t*);
    void* user_data;
    int timeout_ms;
    bool is_async;
    int buffer_size, buffer_size_tx;
    esp_err_t (*crt_bundle_attach)(void*);
    bool keep_alive_enable;
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*);
esp_err_t esp_http_client_set_method(esp_http_client_handle_t,int);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t,const char*,const char*);
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t,const char*,int);
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t,int);
esp_err_t esp_http_client_perform(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
int esp_http_client_get_state(esp_http_client_handle_t);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
