
#ifndef __LVGL_DEMO_H
#define __LVGL_DEMO_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "touch.h"
#include "lcd.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "demos/lv_demos.h"
#include "app_main_ui.h"
#include "app_calculator.h"
#include "app_music.h"
#include "app_video_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_indev_data_t touch_data;
extern uint8_t wifi_app_flag;

/* 函数声明 */
void lvgl_demo(void);

#ifdef __cplusplus
}
#endif

#endif
