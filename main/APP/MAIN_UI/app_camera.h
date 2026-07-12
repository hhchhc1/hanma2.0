/**
 ******************************************************************************
 * @file        app_camera.h
 * @version     V1.0
 * @brief       Camera APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __APP_CAMERA_H__
#define __APP_CAMERA_H__

#include "lvgl.h"
#include <stdio.h>
#include <math.h>
#include "app_main_ui.h"
#include "mipi_cam.h"

/* 摄像头UI结构体 */
typedef struct {
    lv_obj_t * camera_main_ui;      /* 摄像头主界面容器 */
    lv_obj_t * camera_preview;      /* 摄像头预览区域 */
    lv_obj_t * camera_icon;         /* 摄像头图标 */
    lv_obj_t * preview_label;       /* 预览提示文字 */
    lv_obj_t * control_bar;         /* 底部控制栏 */
    lv_obj_t * back_btn;            /* 返回按钮 */
    lv_obj_t * back_label;          /* 返回按钮标签 */
    lv_obj_t * function_label;      /* 功能提示标签 */
} camera_ui_t;

extern camera_ui_t lv_camera_ui;

/* 函数声明 */
void app_camera_exit(void);
void lv_camera_back_event_handler(lv_event_t * e);
void app_camera_ui_init(void);

#endif