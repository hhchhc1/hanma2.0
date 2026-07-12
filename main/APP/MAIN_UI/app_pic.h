/**
 ******************************************************************************
 * @file        app_pic.h
 * @version     V1.0
 * @brief       相册 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __APP_PIC_H__
#define __APP_PIC_H__

#include "lvgl.h"
#include <stdio.h>
#include <math.h>
#include "lcd.h"
#include "app_main_ui.h"
#include "bmp.h"
#include "jpeg.h"
#include "png.h"
#include "exfuns.h"
#include "ff.h"
#include "sdmmc.h"

/* 结构体声明 */
typedef struct {
    lv_obj_t * pic_main_ui;    /* 图片UI容器 */
    lv_obj_t * pic_obj;        /* 图片控件 */
    FF_DIR picdir;
    FILINFO *pic_picfileinfo;
    char *pic_pname;
    uint16_t pic_totpicnum;
    uint16_t pic_curindex;
    uint32_t *pic_picoffsettbl;
    lv_obj_t *pic_frame;
} pic_ui_t;

extern pic_ui_t lv_pic_ui;

extern QueueSetHandle_t    xQueueSet;          /* 定义队列集 */
extern QueueHandle_t       xSemaphore_next;    /* 定义下一个图片开关信号量 */
extern SemaphoreHandle_t   xSemaphore_prev;    /* 定义上一个图片开关信号量 */

/* 函数声明 */
void lv_app_pic_init(void);
FRESULT atk_dir_sdi(FF_DIR *dp, DWORD ofs);
#endif /* __APP_PIC_H__ */