idf_component_register(SRCS "hardware/hal_display.cc" "hardware/hal_storage.cc"
    "hardware/hal_key.cc" "hardware/hal_encoder.cc" "hardware/hal_uart.cc" "hardware/hal_audio_bus.cc"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c
    PRIV_REQUIRES esp_driver_spi esp_lcd esp_driver_gpio esp_driver_sdmmc sdmmc fatfs esp_driver_uart)
