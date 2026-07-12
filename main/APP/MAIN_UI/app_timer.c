/**
 ******************************************************************************
 * @file        app_timer.c
 * @version     V1.0
 * @brief       时钟 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_timer.h"

/* 声明字体 */
LV_FONT_DECLARE(Font144_impact)

/* 初始化全局变量 */
timer_ui_t lv_timer_ui = {0};

/* 星期转换表 */
static const char* weekdays[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

/* 月份转换表 */
static const char* months[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

/**
 * @brief       获取当前时间
 * @param       timeinfo: 时间结构体指针
 * @retval      None
 */
void get_current_time(struct tm *timeinfo)
{
    time_t rawtime;
    time(&rawtime);
    
    /* 使用系统时间，如果有RTC则替换为RTC函数 */
    localtime_r(&rawtime, timeinfo);
    
    /* 如果有RTC硬件，使用以下方式：
     * rtc_get_time(&timeinfo->tm_hour, &timeinfo->tm_min, &timeinfo->tm_sec);
     * rtc_get_date(&timeinfo->tm_year, &timeinfo->tm_mon, &timeinfo->tm_mday);
     * 注意调整年份和月份格式
     */
}

/**
 * @brief       更新时钟显示
 * @param       timer: LVGL定时器指针
 * @retval      None
 */
void timer_update_time(lv_timer_t *timer)
{
    /* 检查UI是否仍然存在 */
    if (lv_timer_ui.timer_label == NULL || !lv_obj_is_valid(lv_timer_ui.timer_label)) {
        lv_timer_del(timer);
        return;
    }
    
    struct tm current_time;
    static char time_str[32];
    static char date_str[32];
    
    get_current_time(&current_time);
    
    /* 只有当时间变化时才更新显示，减少不必要的刷新 */
    if (current_time.tm_sec != lv_timer_ui.last_time.tm_sec) {
        /* 更新时间显示 */
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", 
                current_time.tm_hour, 
                current_time.tm_min, 
                current_time.tm_sec);
        lv_label_set_text(lv_timer_ui.timer_label, time_str);
        
        /* 如果日期或星期变化，更新日期和星期显示 */
        if (current_time.tm_mday != lv_timer_ui.last_time.tm_mday ||
            current_time.tm_wday != lv_timer_ui.last_time.tm_wday ||
            current_time.tm_mon != lv_timer_ui.last_time.tm_mon ||
            current_time.tm_year != lv_timer_ui.last_time.tm_year) {
            
            /* 更新日期显示 */
            snprintf(date_str, sizeof(date_str), "%04d %s %02d", 
                    current_time.tm_year + 1900, 
                    months[current_time.tm_mon],
                    current_time.tm_mday);
            if (lv_timer_ui.date_label && lv_obj_is_valid(lv_timer_ui.date_label)) {
                lv_label_set_text(lv_timer_ui.date_label, date_str);
            }
            
            /* 更新星期显示 */
            if (lv_timer_ui.week_label && lv_obj_is_valid(lv_timer_ui.week_label)) {
                lv_label_set_text(lv_timer_ui.week_label, weekdays[current_time.tm_wday]);
            }
        }
        
        /* 保存当前时间 */
        memcpy(&lv_timer_ui.last_time, &current_time, sizeof(struct tm));
    }
}

/**
 * @brief       删除时钟应用
 * @param       None
 * @retval      None
 */
void lv_app_timer_del(void)
{
    /* 先停止定时器 */
    if (lv_timer_ui.update_timer != NULL) {
        lv_timer_pause(lv_timer_ui.update_timer);  /* 先暂停定时器 */
        lv_timer_del(lv_timer_ui.update_timer);    /* 再删除定时器 */
        lv_timer_ui.update_timer = NULL;
    }
    
    // /* 恢复主应用容器显示 */
    // if (lv_app_ui.main_cont) {
    //     lv_obj_clear_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
    // }
    
    // /* 恢复主应用按钮点击功能 */
    // for (uint8_t index = 0; index < MAIN_APP_NUM; index++) {
    //     if (lv_app_ui.app_main_ui.app_btn[index]) {
    //         lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
    //     }
    // }
    
    // /* 延迟删除UI对象，确保定时器回调不再访问 */
    // if (lv_timer_ui.timer_main_ui) {
    //     /* 使用延迟删除确保安全 */
    //     lv_obj_del_async(lv_timer_ui.timer_main_ui);
    //     lv_timer_ui.timer_main_ui = NULL;
    // }
    
    /* 清空指针 */
    lv_timer_ui.timer_label = NULL;
    lv_timer_ui.date_label = NULL;
    lv_timer_ui.week_label = NULL;
    
    /* 清除上次时间记录 */
    memset(&lv_timer_ui.last_time, 0, sizeof(struct tm));
}

/**
 * @brief       初始化时钟应用
 * @param       None
 * @retval      None
 */
void lv_app_timer_init(void)
{
    struct tm current_time;
    char time_str[32];
    char date_str[32];
    
    // /* 先清理可能存在的旧实例 */
    // if (lv_timer_ui.update_timer != NULL) {
    //     lv_timer_del(lv_timer_ui.update_timer);
    //     lv_timer_ui.update_timer = NULL;
    // }
    
    // if (lv_timer_ui.timer_main_ui != NULL) {
    //     lv_obj_del(lv_timer_ui.timer_main_ui);
    //     lv_timer_ui.timer_main_ui = NULL;
    // }
    
    /* 获取当前时间 */
    get_current_time(&current_time);
    memcpy(&lv_timer_ui.last_time, &current_time, sizeof(struct tm));
    
    /* 隐藏主应用容器 */
    lv_obj_add_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
    
    /* 禁用主应用按钮 */
    for (uint8_t index = 0; index < MAIN_APP_NUM; index++) {
        lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
    }
    
    /* 创建时钟主界面容器 */
    lv_timer_ui.timer_main_ui = lv_obj_create(lv_scr_act());
    
    /* 设置容器样式 */
    lv_obj_set_style_bg_color(lv_timer_ui.timer_main_ui, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_set_size(lv_timer_ui.timer_main_ui, 
                   lv_obj_get_width(lv_scr_act()), 
                   lv_obj_get_height(lv_scr_act()) - lv_obj_get_height(lv_scr_act()) / 20);
    lv_obj_set_style_radius(lv_timer_ui.timer_main_ui, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(lv_timer_ui.timer_main_ui, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_pos(lv_timer_ui.timer_main_ui, 0, lv_obj_get_height(lv_scr_act()) / 20);
    lv_obj_clear_flag(lv_timer_ui.timer_main_ui, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 创建时间显示标签 */
    lv_timer_ui.timer_label = lv_label_create(lv_timer_ui.timer_main_ui);
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", 
            current_time.tm_hour, 
            current_time.tm_min, 
            current_time.tm_sec);
    lv_label_set_text(lv_timer_ui.timer_label, time_str);
    
    /* 设置时间标签样式 */
    lv_obj_set_style_text_font(lv_timer_ui.timer_label, &Font144_impact, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lv_timer_ui.timer_label, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_align(lv_timer_ui.timer_label, LV_ALIGN_CENTER, 0, -50);
    
    /* 创建日期显示标签 */
    lv_timer_ui.date_label = lv_label_create(lv_timer_ui.timer_main_ui);
    snprintf(date_str, sizeof(date_str), "%04d %s %02d", 
            current_time.tm_year + 1900, 
            months[current_time.tm_mon],
            current_time.tm_mday);
    lv_label_set_text(lv_timer_ui.date_label, date_str);
    
    /* 设置日期标签样式 */
    lv_obj_set_style_text_font(lv_timer_ui.date_label, &lv_font_montserrat_36, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lv_timer_ui.date_label, lv_color_hex(0xCCCCCC), LV_STATE_DEFAULT);
    lv_obj_align_to(lv_timer_ui.date_label, lv_timer_ui.timer_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    
    /* 创建星期显示标签 */
    lv_timer_ui.week_label = lv_label_create(lv_timer_ui.timer_main_ui);
    lv_label_set_text(lv_timer_ui.week_label, weekdays[current_time.tm_wday]);
    
    /* 设置星期标签样式 */
    lv_obj_set_style_text_font(lv_timer_ui.week_label, &lv_font_montserrat_48, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lv_timer_ui.week_label, lv_color_hex(0xDDDDDD), LV_STATE_DEFAULT);
    lv_obj_align_to(lv_timer_ui.week_label, lv_timer_ui.date_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    
    /* 创建更新时间定时器（1秒更新一次） */
    lv_timer_ui.update_timer = lv_timer_create(timer_update_time, 1000, NULL);
    
    /* 设置全局变量*/
    lv_general.current_parent = lv_timer_ui.timer_main_ui;
	lv_general.del_function = lv_app_timer_del;  
}