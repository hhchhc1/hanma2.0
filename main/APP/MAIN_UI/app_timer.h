/**
 ******************************************************************************
 * @file        app_timer.h
 * @version     V1.0
 * @brief       时钟 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __APP_TIMER_H__
#define __APP_TIMER_H__

#include "lvgl.h"
#include <stdio.h>
#include <time.h>
#include "app_main_ui.h"

/* 结构体声明 */
typedef struct {
    lv_obj_t * timer_main_ui;    /* 时钟UI容器 */
    lv_obj_t * timer_label;      /* 时间显示标签 */
    lv_obj_t * date_label;       /* 日期显示标签 */
    lv_obj_t * week_label;       /* 星期显示标签 */
    lv_timer_t * update_timer;   /* 更新时间定时器 */
    struct tm last_time;         /* 上次更新时间 */
} timer_ui_t;

extern timer_ui_t lv_timer_ui;

/* 函数声明 */
void lv_app_timer_init(void);
void lv_app_timer_del(void);  
void timer_update_time(lv_timer_t *timer);
void get_current_time(struct tm *timeinfo);

#endif /* __APP_TIMER_H__ */