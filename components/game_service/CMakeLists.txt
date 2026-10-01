idf_component_register(SRCS "game_library.cc" "game_vm.cc" "game_diagnostics.cc" "raster.cc"
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "."
    REQUIRES mosicrokey_contracts PRIV_REQUIRES georgik__lua espressif__cjson esp_psram)
