idf_component_register(SRCS "main.cc" "app_system.cc" REQUIRES mosicrokey_app xiaozhi_compoment espclaw_compoment game_service mosicrokey_ui board_support nvs_flash fatfs PRIV_REQUIRES esp_app_format)
fatfs_create_rawflash_image(system "${PROJECT_DIR}/system" FLASH_IN_PROJECT)
