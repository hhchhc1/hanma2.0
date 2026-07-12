/**
 ******************************************************************************
 * @file        app_calendar.c
 * @version     V1.0
 * @brief       日历 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_calendar.h"
#include "esp_rtc.h"


calendar_ui_t lv_calendar_ui;

static lv_calendar_date_t highlight_days[1]; /* 定义的日期,必须用全局或静态定义 */
static const char * years = "2030\n2029\n2028\n2027\n2026\n2025\n2024";


void app_calendar_ui_init(void)
{
    lv_calendar_ui.calendar_main_ui = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(lv_calendar_ui.calendar_main_ui, lv_color_hex(0x000000), LV_STATE_DEFAULT);                  /* 设置背景颜色 */
    lv_obj_set_size(lv_calendar_ui.calendar_main_ui, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()) - lv_obj_get_height(lv_scr_act()) / 20);  /* 设置容器大小 */
    lv_obj_set_style_radius(lv_calendar_ui.calendar_main_ui, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
    lv_obj_set_style_border_opa(lv_calendar_ui.calendar_main_ui, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
    lv_obj_set_pos(lv_calendar_ui.calendar_main_ui, 0, lv_obj_get_height(lv_scr_act()) / 20);                              /* 设置位置 */
    lv_obj_clear_flag(lv_calendar_ui.calendar_main_ui, LV_OBJ_FLAG_SCROLLABLE);                                            /* 禁止滚动 */
    lv_obj_update_layout(lv_calendar_ui.calendar_main_ui);

    /* 定义并初始化日历 */
    lv_calendar_ui.calendar_obj = lv_calendar_create(lv_calendar_ui.calendar_main_ui);
    /* 设置日历的大小 */
    lv_obj_set_size(lv_calendar_ui.calendar_obj, lv_obj_get_width(lv_calendar_ui.calendar_main_ui) ,lv_obj_get_height(lv_calendar_ui.calendar_main_ui));
    lv_obj_set_style_radius(lv_calendar_ui.calendar_obj, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
    lv_obj_set_style_border_opa(lv_calendar_ui.calendar_obj, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
    lv_obj_center(lv_calendar_ui.calendar_obj);

    /* 设置日历头 */
    lv_calendar_header_dropdown_create(lv_calendar_ui.calendar_obj);
    lv_calendar_header_dropdown_set_year_list(lv_calendar_ui.calendar_obj,years);

    /* 设置日历的日期 */
    lv_calendar_set_today_date(lv_calendar_ui.calendar_obj, 2026, 1, 1);
    /* 设置日历显示的月份 */
    lv_calendar_set_showed_date(lv_calendar_ui.calendar_obj, 2026, 1);    

    highlight_days[0].year = 2026;
    highlight_days[0].month = 1;
    highlight_days[0].day = 1;

    lv_calendar_set_highlighted_dates(lv_calendar_ui.calendar_obj, highlight_days,1);
    /* 更新日历参数 */
    lv_obj_update_layout(lv_calendar_ui.calendar_obj);

    lv_general.current_parent = lv_calendar_ui.calendar_main_ui;
}