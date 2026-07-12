/**
 ******************************************************************************
 * @file        app_2048.h
 * @version     V1.0
 * @brief       2048 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef _APP_2048_H
#define _APP_2048_H

#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "app_main_ui.h"
#include "lvgl_demo.h"
#include "lcd.h"
#include <inttypes.h>

/* 方向枚举 */
typedef enum
{
    up,
    down,
    left,
    right,
    none,
} direction_type_enum;

/* 方块结构体 */
typedef struct
{
    lv_obj_t *cube_obj;
    int32_t value;
} cube_type_def;

/* 2048 UI结构体 */
typedef struct {
    lv_obj_t *game_main_ui;    /* 游戏主UI容器 */
    lv_obj_t *exit_btn;        /* 退出按钮 */
} game2048_ui_t;

extern game2048_ui_t lv_2048_ui;

/* 函数声明 */
void app_2048_ui_init(void);
static void game_start(lv_event_t *e);
static void game_exit(lv_event_t *e);
static void game_reset(void);
static void move_obj_cb(lv_event_t *e);
static void set_x_cb(void *var, int32_t v);
static void set_y_cb(void *var, int32_t v);
static void lv_myanim_creat(void *var, lv_anim_exec_xcb_t exec_cb, uint32_t time, uint32_t delay, uint32_t playback_time,
                            int32_t start, int32_t end, void *user_data, lv_anim_deleted_cb_t deleted_cb);
static int32_t color_index(int32_t num);
static void move_done_cb(lv_anim_t *a);
static void released_cb(lv_event_t *e);
static void new_cube_creat(int32_t i, int32_t j);
static bool check_game_over(void);

#endif