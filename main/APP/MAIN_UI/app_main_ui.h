/**
 ******************************************************************************
 * @file        app_main_ui.h
 * @version     V1.0
 * @brief       主界面UI
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __APP_MAIN_UI_H__
#define __APP_MAIN_UI_H__

#include "lvgl.h"
#include <stdio.h>
#include "handle.h"
#include "esp_rtc.h"
#include "app_calculator.h"
#include "app_music.h"

#define MAIN_APP_NUM        12   /* 定义主图 APP数量 */

/* 定义描述APP信息的结构体 */
typedef struct
{
    uint8_t  app_id;            /* app ID */
    char * app_text_en;         /* app 英文名称 */
    char * app_text_cn;         /* app 中文名称 */
    const lv_img_dsc_t * icon_image;  /* app 图标源 */
}app_info_t;

/* 定义LVGL APP界面所需的部件结构体 */
typedef struct
{
    lv_obj_t *main_cont;                                        /* 主界面容器 */
    lv_obj_t *tileview;                                         /* TileView 页面切换 */
    lv_obj_t *tile_v[3];                                        /* Tile: 0=主页, 1=Smart Home, 2=AI Chat */
    lv_obj_t *page_dots[3];                                     /* 页面指示器小圆点 */
    uint8_t current_tile;                                       /* 当前激活的 tile */

    struct
    {
        lv_obj_t * ico_box_cont;                                /* 管理小容器 */
        lv_obj_t * app_btn[MAIN_APP_NUM];                       /* app按键数组 */
        lv_obj_t * app_name[MAIN_APP_NUM];                      /* app名称数组 */
		lv_obj_t *main_time_label;  // 主界面时间显示标签
    }app_main_ui;

    struct
    {
        lv_obj_t * small_cont;                                  /* 显示小容器 */
        lv_obj_t * small_cont_sd;                               /* SD卡图标 */
        lv_obj_t * small_cont_usb;                              /* usb图标 */
        lv_obj_t * small_cont_power;                            /* 电源图标 */
        lv_obj_t * small_cont_timer;                            /* 时间图标 */

        lv_obj_t * small_cont_manage;                           /* 管理图标 */
        lv_obj_t * small_cont_brightness;                       /* 亮度图标 */
        lv_obj_t * small_cont_voice;                            /* 音量调节 */
        lv_obj_t * small_cont_flexo[4];                         /* 多功能图标容器 */
        lv_obj_t * small_cont_flexo_ico[4];                     /* 多功能图标 */
    }app_small_ui;

    struct
    {
        lv_timer_t* lv_rtc_timer;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        uint8_t year;
        uint8_t month;
        uint8_t date;
        uint8_t week;
        char rtc_tbuf[40];
    }rtc;

}app_ui_t;

/* SD状态值 */
typedef enum
{
    SD_CONNET,
    SD_DISCONNECT,
}SD_STATE;

/* SD状态值 */
typedef enum
{
    USB_JTAG_CONNET,
    USB_JTAG_DISCONNECT,
}USB_JTAG_STATE;

/* 子界面返回至主界面控制结构体 */
typedef struct
{
    lv_obj_t * current_parent;                                  /* 指向当前父对象 */
    lv_obj_t * del_parent;                                      /* 指向当前要删除的对象 */
    SD_STATE sd_check_en;                                        /* SD卡状态 */
	USB_JTAG_STATE usb_jtag_check_en;                            /* USB JTAG状态 */
    void (*del_function)(void);                                 /* 指向删除回调函数 */
}grneral_t;

extern grneral_t lv_general;
extern app_ui_t lv_app_ui;

/* 函数声明 */
void lv_app_main_ui_init(void);
void lv_anim_y_act(lv_obj_t * parent, int32_t v);
void lv_anim_h_act(lv_obj_t * parent, int32_t v,bool flag);
void lv_pull_ui(lv_obj_t * parent);
void lv_msgbox(char *name);

#endif
