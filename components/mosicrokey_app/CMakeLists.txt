idf_component_register(SRCS "app.cc" "console.cc" "task/app_task.cc" "task/key_task.cc" "task/console_task.cc"
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "."
    REQUIRES mosicrokey_contracts
    PRIV_REQUIRES board_support mosicrokey_ui esp_psram esp_netif esp_timer esp_app_format espressif__cjson mbedtls)
