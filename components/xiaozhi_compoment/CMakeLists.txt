set(XZ "${PROJECT_DIR}/third_party/xiaozhi")
set(XM "${XZ}/main")
set(XZ_SOURCES
    application.cc assets.cc ota.cc settings.cc system_info.cc mcp_server.cc device_state_machine.cc
    audio/audio_codec.cc audio/audio_debugger.cc audio/audio_service.cc audio/demuxer/ogg_demuxer.cc
    audio/codecs/es8311_audio_codec.cc audio/engines/afe_audio_engine.cc
    audio/wake_words/custom_wake_word.cc audio/wake_words/wake_word_audio_cache.cc
    notify/notify_player.cc led/single_led.cc display/display.cc display/text_glyph.cc
    display/lvgl_display/emoji_collection.cc display/lvgl_display/lvgl_theme.cc
    display/lvgl_display/lvgl_font.cc display/lvgl_display/dynamic_glyph_cache.cc
    display/lvgl_display/lvgl_image.cc display/lvgl_display/gif/lvgl_gif.cc
    display/lvgl_display/gif/gifdec.c
    protocols/protocol.cc protocols/text_glyph_payload.cc protocols/mqtt_protocol.cc protocols/websocket_protocol.cc
    boards/common/board.cc boards/common/wifi_board.cc boards/common/button.cc
    boards/common/backlight.cc boards/common/system_reset.cc)
list(TRANSFORM XZ_SOURCES PREPEND "${XM}/")
file(GLOB ZH_SOUNDS "${XM}/assets/locales/zh-CN/*.ogg")
file(GLOB EN_SOUNDS "${XM}/assets/locales/en-US/*.ogg")
file(GLOB COMMON_SOUNDS "${XM}/assets/common/*.ogg")
set(SOUNDS ${ZH_SOUNDS} ${COMMON_SOUNDS})
foreach(SOUND IN LISTS EN_SOUNDS)
    get_filename_component(NAME "${SOUND}" NAME)
    if(NOT EXISTS "${XM}/assets/locales/zh-CN/${NAME}")
        list(APPEND SOUNDS "${SOUND}")
    endif()
endforeach()
set(GENERATED "${CMAKE_BINARY_DIR}/mosicrokey_generated")
file(MAKE_DIRECTORY "${GENERATED}")
idf_component_register(SRCS ${XZ_SOURCES} "xiaozhi_adapter.cc" "mosicrokey_board.cc"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "${GENERATED}" "${XM}" "${XM}/display" "${XM}/display/lvgl_display"
        "${XM}/display/lvgl_display/jpg" "${XM}/audio" "${XM}/audio/demuxer" "${XM}/protocols" "${XM}/boards/common"
    EMBED_FILES ${SOUNDS}
    REQUIRES mosicrokey_contracts board_support
    PRIV_REQUIRES esp_pm esp_psram esp_netif esp_driver_gpio esp_driver_spi esp_driver_i2c
        esp_driver_i2s esp_driver_ledc esp_driver_jpeg esp_driver_tsens esp_app_format app_update
        spi_flash mbedtls console efuse esp_wifi nvs_flash esp-tls espressif__cjson pthread
        78__esp-ml307
        78__esp-wifi-connect 78__xiaozhi-fonts espressif__esp_audio_effects espressif__esp_audio_codec
        espressif__esp_codec_dev espressif__esp-sr espressif__led_strip espressif__button
        espressif__esp_new_jpeg lvgl__lvgl)
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++23>
        -Wno-missing-field-initializers)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BOARD_TYPE="mosicrokey-s31" BOARD_NAME="mosicrokey-s31"
        BOARD_MANUFACTURER="mosicrokey" BUILTIN_TEXT_FONT=font_noto_sans_basic_14_1
        BUILTIN_ICON_FONT=font_material_symbols_14_1 TEXT_FONT_SIZE=14 TEXT_FONT_BPP=1
        HAVE_LVGL=1 MOSICROKEY_PRODUCT=1)
    add_custom_command(OUTPUT "${GENERATED}/assets/lang_config.h"
        COMMAND ${PYTHON} "${PROJECT_DIR}/scripts/generate_language.py" "${XM}/assets" "${GENERATED}/assets/lang_config.h"
        DEPENDS "${PROJECT_DIR}/scripts/generate_language.py" "${XM}/assets/locales/zh-CN/language.json")
    add_custom_target(mosicrokey_language DEPENDS "${GENERATED}/assets/lang_config.h")
    add_dependencies(${COMPONENT_LIB} mosicrokey_language)
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/assets.bin"
        COMMAND ${PYTHON} "${XZ}/scripts/build_default_assets.py" --sdkconfig "${SDKCONFIG}"
            --esp_sr_model_path "${PROJECT_DIR}/managed_components/espressif__esp-sr/model"
            --noto_fonts_path "${PROJECT_DIR}/managed_components/78__xiaozhi-fonts"
            --builtin_text_font font_noto_sans_basic_14_1 --output "${CMAKE_BINARY_DIR}/assets.bin"
        DEPENDS "${SDKCONFIG}" "${XZ}/scripts/build_default_assets.py")
    add_custom_target(mosicrokey_assets ALL DEPENDS "${CMAKE_BINARY_DIR}/assets.bin")
    esptool_py_flash_to_partition(flash "assets" "${CMAKE_BINARY_DIR}/assets.bin")
endif()
