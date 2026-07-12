/**
 ******************************************************************************
 * @file        app_calendar.h
 * @version     V1.0
 * @brief       日历 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __APP_CALENDAR_H__
#define __APP_CALENDAR_H__

#include "lvgl.h"
#include <stdio.h>
#include <math.h>
#include "app_main_ui.h"
#include "esp_rtc.h"


/* 结构体声明 */
typedef struct {
    lv_obj_t * calendar_main_ui;    /* 日历UI容器 */
    lv_obj_t * calendar_obj;        /* 日历控件 */
    lv_obj_t * calendar_msgbox;     /* 设置日历 */
} calendar_ui_t;

extern calendar_ui_t calendar_ui;

/* 函数声明 */
void app_calendar_ui_init(void);

#endif
