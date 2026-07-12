set(src_dirs
            LED
            LCD
            MYIIC
            TOUCH
            MYES8311
            LEDC
            MYI2S
            SDMMC
            RTC
            TWAI
            KEY
			SPILCD)

set(include_dirs
            LED
            LCD
            MYIIC
            TOUCH
            MYES8311
            LEDC
            MYI2S
            SDMMC
            RTC
            TWAI
            KEY
			SPILCD)

set(requires
            driver
            esp_lcd
            esp_common
            lvgl__lvgl
            fatfs
            newlib
            esp_driver_touch_sens
			es8311
			esp_codec_dev
			esp_lcd_st7796)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)