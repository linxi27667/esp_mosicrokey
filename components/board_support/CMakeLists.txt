idf_component_register(SRCS "board_support.cc" INCLUDE_DIRS "include"
    REQUIRES mosicrokey_contracts
    PRIV_REQUIRES esp_driver_spi esp_lcd esp_driver_gpio esp_driver_sdmmc sdmmc fatfs esp_driver_ledc)
