idf_component_register(SRCS "app.cc" "console.cc" INCLUDE_DIRS "include"
    REQUIRES mosicrokey_contracts
    PRIV_REQUIRES board_support mosicrokey_ui console esp_psram esp_netif esp_timer esp_driver_uart esp_app_format espressif__cjson mbedtls)
