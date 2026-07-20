set(SOURCES "audio/audio_codec.cc"
            "audio/audio_service.cc"
            "audio/codecs/no_audio_codec.cc"
            "audio/codecs/box_audio_codec.cc"
            "audio/codecs/es8311_audio_codec.cc"
            "audio/codecs/es8374_audio_codec.cc"
            "audio/codecs/es8388_audio_codec.cc"
            "audio/codecs/es8389_audio_codec.cc"
            "audio/codecs/dummy_audio_codec.cc"
            "audio/processors/audio_debugger.cc"
            "led/single_led.cc"
            "led/circular_strip.cc"
            "led/gpio_led.cc"
            "display/display.cc"
            "display/lcd_display.cc"
            "display/oled_display.cc"
            "display/extra_screens.cc"
            "display/wifi_settings.cc"
            "sensors/dht22.c"
            "sensors/bh1750.c"
            "protocols/protocol.cc"
            "protocols/mqtt_protocol.cc"
            "protocols/websocket_protocol.cc"
            "mcp_server.cc"
            "smart_home_web_server.cc"
            "system_info.cc"
            "application.cc"
            "ota.cc"
            "settings.cc"
            "device_state_event.cc"
            "input/ec11.cc"
            "main.cc"
            "esp_network.cc"
            "http_client.cc"
            "esp_mqtt.cc"
            "esp_udp.cc"
            "esp_websocket.cc"
            "mem_protect/mem_pool.cc"
            "matter_device/matter_device_mgr.cc"
            )

set(INCLUDE_DIRS "." "display" "audio" "protocols" "sensors" "input"
    "posix_compat" "mem_protect" "matter_device"
    "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/espressif__esp_cam_sensor/sensors/gc0308/include")

# Board common files
file(GLOB BOARD_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/boards/common/*.cc)
list(APPEND SOURCES ${BOARD_COMMON_SOURCES})
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/common)

# Remove board-specific files that depend on missing components for the current target
if(CONFIG_BOARD_TYPE_WKS_ESP32P4_CB)
    # wks-p4-cb is a WiFi board, no ML307 4G modem
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/boards/common/ml307_board.cc")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/boards/common/dual_network_board.cc")
endif()

# Set BOARD_TYPE from Kconfig
if(CONFIG_BOARD_TYPE_WKS_ESP32P4_CB)
    set(BOARD_TYPE "wks-p4-cb")
endif()

file(GLOB BOARD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.c
)
list(APPEND SOURCES ${BOARD_SOURCES})

# Camera sources (ESP32-P4 with MIPI CSI)
if(CONFIG_IDF_TARGET_ESP32P4)
    list(APPEND SOURCES "camera/app_video.c" "camera/camera_display.cc" "camera/gpio_sccb.c")
endif()

# Add board-specific include directory for config.h access
list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE})

if(CONFIG_USE_AUDIO_PROCESSOR)
    list(APPEND SOURCES "audio/processors/afe_audio_processor.cc")
else()
    list(APPEND SOURCES "audio/processors/no_audio_processor.cc")
endif()
if(CONFIG_USE_AFE_WAKE_WORD)
    list(APPEND SOURCES "audio/wake_words/afe_wake_word.cc")
elseif(CONFIG_USE_ESP_WAKE_WORD)
    list(APPEND SOURCES "audio/wake_words/esp_wake_word.cc")
elseif(CONFIG_USE_CUSTOM_WAKE_WORD)
    list(APPEND SOURCES "audio/wake_words/custom_wake_word.cc")
endif()

# Language selection
if(CONFIG_LANGUAGE_ZH_CN)
    set(LANG_DIR "zh-CN")
elseif(CONFIG_LANGUAGE_ZH_TW)
    set(LANG_DIR "zh-TW")
elseif(CONFIG_LANGUAGE_EN_US)
    set(LANG_DIR "en-US")
elseif(CONFIG_LANGUAGE_JA_JP)
    set(LANG_DIR "ja-JP")
elseif(CONFIG_LANGUAGE_KO_KR)
    set(LANG_DIR "ko-KR")
elseif(CONFIG_LANGUAGE_VI_VN)
    set(LANG_DIR "vi-VN")
elseif(CONFIG_LANGUAGE_TH_TH)
    set(LANG_DIR "th-TH")
elseif(CONFIG_LANGUAGE_DE_DE)
    set(LANG_DIR "de-DE")
elseif(CONFIG_LANGUAGE_FR_FR)
    set(LANG_DIR "fr-FR")
elseif(CONFIG_LANGUAGE_ES_ES)
    set(LANG_DIR "es-ES")
elseif(CONFIG_LANGUAGE_IT_IT)
    set(LANG_DIR "it-IT")
elseif(CONFIG_LANGUAGE_RU_RU)
    set(LANG_DIR "ru-RU")
elseif(CONFIG_LANGUAGE_AR_SA)
    set(LANG_DIR "ar-SA")
elseif(CONFIG_LANGUAGE_HI_IN)
    set(LANG_DIR "hi-IN")
elseif(CONFIG_LANGUAGE_PT_PT)
    set(LANG_DIR "pt-PT")
elseif(CONFIG_LANGUAGE_PL_PL)
    set(LANG_DIR "pl-PL")
elseif(CONFIG_LANGUAGE_CS_CZ)
    set(LANG_DIR "cs-CZ")
elseif(CONFIG_LANGUAGE_FI_FI)
    set(LANG_DIR "fi-FI")
elseif(CONFIG_LANGUAGE_TR_TR)
    set(LANG_DIR "tr-TR")
elseif(CONFIG_LANGUAGE_ID_ID)
    set(LANG_DIR "id-ID")
elseif(CONFIG_LANGUAGE_UK_UA)
    set(LANG_DIR "uk-UA")
elseif(CONFIG_LANGUAGE_RO_RO)
    set(LANG_DIR "ro-RO")
endif()

# Define generation paths
set(LANG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/${LANG_DIR}/language.json")
set(LANG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/assets/lang_config.h")
file(GLOB LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/${LANG_DIR}/*.ogg)
file(GLOB COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/common/*.ogg)

# Generate TTS PCM files placeholder
set(TTS_PCM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/common)
if(NOT EXISTS "${TTS_PCM_DIR}/dang_qian_wen_du.pcm")
    file(WRITE "${TTS_PCM_DIR}/dang_qian_wen_du.pcm" "")
endif()
if(NOT EXISTS "${TTS_PCM_DIR}/she_shi_du.pcm")
    file(WRITE "${TTS_PCM_DIR}/she_shi_du.pcm" "")
endif()
if(NOT EXISTS "${TTS_PCM_DIR}/dang_qian_shi_du.pcm")
    file(WRITE "${TTS_PCM_DIR}/dang_qian_shi_du.pcm" "")
endif()
if(NOT EXISTS "${TTS_PCM_DIR}/bai_fen_bi.pcm")
    file(WRITE "${TTS_PCM_DIR}/bai_fen_bi.pcm" "")
endif()
if(NOT EXISTS "${TTS_PCM_DIR}/ti_xing_shi_jian_dao.pcm")
    file(WRITE "${TTS_PCM_DIR}/ti_xing_shi_jian_dao.pcm" "")
endif()
if(NOT EXISTS "${TTS_PCM_DIR}/alarm.pcm")
    file(WRITE "${TTS_PCM_DIR}/alarm.pcm" "")
endif()
file(GLOB COMMON_PCMS ${CMAKE_CURRENT_SOURCE_DIR}/assets/common/*.pcm)

# Register component
idf_component_register(SRCS ${SOURCES}
                    EMBED_FILES ${LANG_SOUNDS} ${COMMON_SOUNDS} ${COMMON_PCMS}
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                     REQUIRES esp_driver_ppa esp_new_jpeg esp_app_format
                             esp_http_client mbedtls esp_partition esp_psram
                             app_update pthread efuse spi_flash esp_cam_sensor
                             esp_websocket_client mqtt lwip esp-tls
                    WHOLE_ARCHIVE
                    )

# Kconfig workaround: see project root CMakeLists.txt for wifi_remote defines

# BOARD_TYPE compile definition
if(NOT BOARD_NAME)
    set(BOARD_NAME ${BOARD_TYPE})
endif()
target_compile_definitions(${COMPONENT_LIB}
                    PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\" BOARD_NAME=\"${BOARD_NAME}\"
                    )

# Generate language config header
add_custom_command(
    OUTPUT ${LANG_HEADER}
    COMMAND python ${PROJECT_DIR}/scripts/gen_lang.py
            --language "${LANG_DIR}"
            --output "${LANG_HEADER}"
    DEPENDS
        ${LANG_JSON}
        ${PROJECT_DIR}/scripts/gen_lang.py
    COMMENT "Generating ${LANG_DIR} language config"
)

add_custom_target(lang_header ALL
    DEPENDS ${LANG_HEADER}
)
