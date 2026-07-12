#ifndef __HANDLE_H__
#define __HANDLE_H__

#include "lvgl.h"
#include <stdio.h>
#include "app_main_ui.h"
#include "driver/ledc.h"
#include "myes8311.h"
#include "key.h"
#include "led.h"
#include "my_esp_twai.h"
#include "esp_rtc.h"
#include "app_calculator.h"
#include "esp_lvgl_port.h"
#include "esp_lvgl_port_disp.h"
#include "sdmmc.h"
#include "app_calendar.h"
#include "app_pic.h"
#include "app_music.h"
#include "app_video_ui.h"
#include "app_brush.h"
#include "app_camera.h"
#include "app_2048.h"
#include "app_file.h"
#include "app_wifi.h"
#include "app_usb_otg.h"
#include "app_timer.h"


/* 操作亮度与音量函数 */
void lv_menu_interface_event_cb(lv_event_t *event);
void lv_imgbtn_control_event_handler(lv_event_t *event);
void lv_background_data_processing_timer(lv_timer_t* timer);
void lv_scr_event_cb(lv_event_t *event);
void lv_ui_del(SemaphoreHandle_t BinarySemaphore);
void lv_mbox_event_cb(lv_event_t * e);

esp_err_t bsp_display_brightness_set(int brightness_percent);
esp_err_t bsp_display_voice_set(int voice_percent);
#endif