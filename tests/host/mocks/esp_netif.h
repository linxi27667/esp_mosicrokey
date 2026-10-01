#pragma once
struct esp_netif_t {};
struct esp_netif_ip_info_t {struct {unsigned addr=0;} ip;};
constexpr int ESP_OK=0;
inline esp_netif_t* esp_netif_get_handle_from_ifkey(const char*) {return nullptr;}
inline int esp_netif_get_ip_info(esp_netif_t*,esp_netif_ip_info_t*) {return -1;}
