/**
 ******************************************************************************
 * @file        app_2048.c
 * @version     V1.0
 * @brief       2048 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_2048.h"

LV_IMG_DECLARE(game2048_start_img)

/* 全局变量定义 */
static lv_point_t click_point1, click_point2;
static int32_t movex, movey, moving_count;
static direction_type_enum direction = none;
static bool touched = false, released = true;
static const uint32_t color[] = {0xfaf8ef, 0xece2ca, 0xf4b278, 0xf69662, 0xf47c60, 0xfc5c3e, 0xf0cf71, 0xefcd60, 0xefc84d, 0xe4bc12, 0xedc900};
static cube_type_def cube[4][4];
static lv_obj_t *screen, *bg, *start_btn, *touch_layer;
static float screen_ratio;
static int32_t screen_w, screen_h, screen_size;

/* 2048 UI结构体 */
game2048_ui_t lv_2048_ui;

void app_2048_ui_init(void)
{
    /* 获取屏幕尺寸 */
    screen_w = lv_disp_get_hor_res(lv_disp_get_default());
    screen_h = lv_disp_get_ver_res(lv_disp_get_default());

    /* 计算屏幕尺寸和比例 */
    if (screen_w >= screen_h)
    {
        screen_size = screen_h;
    }
    else
    {
        screen_size = screen_w;
    }
    screen_ratio = (float)screen_size / 600;

    /* 创建主UI容器 */
    lv_2048_ui.game_main_ui = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(lv_2048_ui.game_main_ui, lv_color_hex(0xfaf8ef), LV_STATE_DEFAULT);                  /* 设置背景颜色 */
    lv_obj_set_size(lv_2048_ui.game_main_ui, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()) - lv_obj_get_height(lv_scr_act()) / 20);  /* 设置容器大小 */
    lv_obj_set_style_radius(lv_2048_ui.game_main_ui, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
    lv_obj_set_style_border_opa(lv_2048_ui.game_main_ui, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
    lv_obj_set_pos(lv_2048_ui.game_main_ui, 0, lv_obj_get_height(lv_scr_act()) / 20);                              /* 设置位置 */
    lv_obj_clear_flag(lv_2048_ui.game_main_ui, LV_OBJ_FLAG_SCROLLABLE);                                            /* 禁止滚动 */
    lv_obj_update_layout(lv_2048_ui.game_main_ui);

    /* 创建游戏开始界面 */
    screen = lv_tileview_create(lv_2048_ui.game_main_ui);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xfaf8ef), LV_PART_MAIN);

    /* 创建开始按钮 */
    start_btn = lv_img_create(screen);
    lv_img_set_src(start_btn, &game2048_start_img);
    lv_obj_center(start_btn);
    lv_img_set_zoom(start_btn, 256 * screen_ratio);
    lv_obj_add_flag(start_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(start_btn, game_start, LV_EVENT_RELEASED, 0);

    /* 创建退出按钮 */
	lv_2048_ui.exit_btn = lv_btn_create(lv_2048_ui.game_main_ui);
	lv_obj_set_size(lv_2048_ui.exit_btn, 60, 60);
	lv_obj_align(lv_2048_ui.exit_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
	lv_obj_set_style_bg_opa(lv_2048_ui.exit_btn, 1, LV_STATE_DEFAULT);
	lv_obj_t *exit_label = lv_label_create(lv_2048_ui.exit_btn);
	lv_obj_set_style_text_font(exit_label, &lv_font_montserrat_36, LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(exit_label, lv_color_hex(0x000000), LV_STATE_DEFAULT);
	lv_label_set_text(exit_label, LV_SYMBOL_CLOSE);
	lv_obj_center(exit_label);
	lv_obj_add_event_cb(lv_2048_ui.exit_btn, game_exit, LV_EVENT_CLICKED, NULL);

    lv_general.current_parent = lv_2048_ui.game_main_ui;
}

/* 退出游戏事件处理 */
static void game_exit(lv_event_t *e)
{
    /* 删除游戏UI容器 */
    if (lv_2048_ui.game_main_ui != NULL)
    {
        lv_obj_del(lv_2048_ui.game_main_ui);
        lv_2048_ui.game_main_ui = NULL;
    }
    
    /* 重置游戏状态 */
    game_reset();
    
    /* 返回主界面 */
    lv_general.current_parent = NULL;
}

/* 重置游戏状态 */
static void game_reset(void)
{
    /* 重置所有方块状态 */
    lv_memset_00(cube, sizeof(cube));
    moving_count = 0;
    direction = none;
    touched = false;
    released = true;
}

static void game_start(lv_event_t *e)
{
    int i, j;
    
    /* 初始化游戏数据 */
    lv_memset_00(cube, sizeof(cube));
    moving_count = 0;

    /* 创建游戏背景 */
    bg = lv_tileview_create(screen);
    lv_obj_center(bg);
    lv_obj_set_size(bg, screen_size * 0.9, screen_size * 0.9);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0xbbada0), LV_PART_MAIN);

    /* 创建游戏格子背景 */
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            lv_obj_t *bg_cube = lv_obj_create(bg);
            lv_obj_align(bg_cube, LV_ALIGN_TOP_LEFT, 12 * screen_ratio + 131 * j * screen_ratio, 12 * screen_ratio + 131 * i * screen_ratio);
            lv_obj_set_size(bg_cube, 121 * screen_ratio, 121 * screen_ratio);
            lv_obj_set_style_bg_color(bg_cube, lv_color_hex(0xcdc1b4), LV_PART_MAIN);
            lv_obj_set_style_border_width(bg_cube, 0, LV_PART_MAIN);
        }
    }

    /* 创建初始方块 */
    for (int32_t i = 1; i < 3; i++)
    {
        new_cube_creat(i, i);
    }

    /* 创建触摸层 */
    touch_layer = lv_tileview_create(screen);
    lv_obj_clear_flag(touch_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(touch_layer, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(touch_layer, move_obj_cb, LV_EVENT_PRESSING, 0);
    lv_obj_add_event_cb(touch_layer, released_cb, LV_EVENT_RELEASED, 0);
}

static void released_cb(lv_event_t *e)
{
    touched = false;
    released = true;
}

static void move_obj_cb(lv_event_t *e)
{
    if (touched == false)
    {
        touched = true;
        lv_indev_get_point(lv_indev_get_act(), &click_point1);
        return;
    }
    
    lv_indev_get_point(lv_indev_get_act(), &click_point2);
    movex = click_point2.x - click_point1.x;
    movey = click_point2.y - click_point1.y;
    
    if ((movex * movex + movey * movey < 50)) return;
    
    /* 判断移动方向 */
    if ((movex <= 0 && movey <= 0 && movex > movey) || (movex >= 0 && movey <= 0 && movex < -movey)) { direction = up; }
    if ((movex >= 0 && movey <= 0 && movex > -movey) || (movex >= 0 && movey >= 0 && movex > movey)) { direction = right; }
    if ((movex <= 0 && movey <= 0 && movex < movey) || (movex <= 0 && movey >= 0 && movex < -movey)) { direction = left; }
    if ((movex <= 0 && movey >= 0 && movex > -movey) || (movex >= 0 && movey >= 0 && movex < movey)) { direction = down; }

    if (moving_count == 0 && released == true)
    {
        released = false;

        /* 向上移动逻辑 */
        if (direction == up)
        {
            for (int32_t j = 0; j < 4; j++)
            {
                for (int32_t i = 0; i < 3; i++)
                {
                    if (cube[i][j].value)
                    {
                        for (int32_t k = i + 1; k < 4; k++)
                        {
                            if (cube[k][j].value == 0) continue;
                            if (cube[k][j].value != cube[i][j].value) break;
                            if (cube[k][j].value == cube[i][j].value)
                            {
                                cube[i][j].value += cube[k][j].value;
                                cube[k][j].value = 0;
                                lv_myanim_creat(cube[k][j].cube_obj, set_y_cb, 100, 0, 0, -198 * screen_ratio + 131 * k * screen_ratio, -198 * screen_ratio + 131 * i * screen_ratio, &cube[i][j], move_done_cb);
                                moving_count++; break;
                            }
                        }
                    }
                    else
                    {
                        for (int32_t k = i + 1; k < 4; k++)
                        {
                            if (cube[k][j].value == 0) continue;
                            else
                            {
                                cube[i][j].value = cube[k][j].value;
                                cube[i][j].cube_obj = cube[k][j].cube_obj;
                                cube[k][j].value = 0;
                                lv_myanim_creat(cube[i][j].cube_obj, set_y_cb, 100, 0, 0, -198 * screen_ratio + 131 * k * screen_ratio, -198 * screen_ratio + 131 * i * screen_ratio, 0, move_done_cb);
                                i--; moving_count++; break;
                            }
                        }
                    }
                }
            }
            return;
        }

        /* 向下移动逻辑 */
        if (direction == down)
        {
            for (int32_t j = 0; j < 4; j++)
            {
                for (int32_t i = 3; i > 0; i--)
                {
                    if (cube[i][j].value)
                    {
                        for (int32_t k = i - 1; k >= 0; k--)
                        {
                            if (cube[k][j].value == 0) continue;
                            if (cube[k][j].value != cube[i][j].value) break;
                            if (cube[k][j].value == cube[i][j].value)
                            {
                                cube[i][j].value += cube[k][j].value;
                                cube[k][j].value = 0;
                                lv_myanim_creat(cube[k][j].cube_obj, set_y_cb, 100, 0, 0, -198 * screen_ratio + 131 * k * screen_ratio, -198 * screen_ratio + 131 * i * screen_ratio, &cube[i][j], move_done_cb);
                                moving_count++; break;
                            }
                        }
                    }
                    else
                    {
                        for (int32_t k = i - 1; k >= 0; k--)
                        {
                            if (cube[k][j].value == 0) continue;
                            else
                            {
                                cube[i][j].value = cube[k][j].value;
                                cube[i][j].cube_obj = cube[k][j].cube_obj;
                                cube[k][j].value = 0;
                                lv_myanim_creat(cube[i][j].cube_obj, set_y_cb, 100, 0, 0, -198 * screen_ratio + 131 * k * screen_ratio, -198 * screen_ratio + 131 * i * screen_ratio, 0, move_done_cb);
                                i++; moving_count++; break;
                            }
                        }
                    }
                }
            }
            return;
        }

        /* 向左移动逻辑 */
        if (direction == left)
        {
            for (int32_t i = 0; i < 4; i++)
            {
                for (int32_t j = 0; j < 3; j++)
                {
                    if (cube[i][j].value)
                    {
                        for (int32_t k = j + 1; k < 4; k++)
                        {
                            if (cube[i][k].value == 0) continue;
                            if (cube[i][k].value != cube[i][j].value) break;
                            if (cube[i][k].value == cube[i][j].value)
                            {
                                cube[i][j].value += cube[i][k].value;
                                cube[i][k].value = 0;
                                lv_myanim_creat(cube[i][k].cube_obj, set_x_cb, 100, 0, 0, -198 * screen_ratio + 131 * k * screen_ratio, -198 * screen_ratio + 131 * j * screen_ratio, &cube[i][j], move_done_cb);
                                moving_count++; break;
                            }
                        }
                    }
                    else
                    {
                        for (int32_t k = j + 1; k < 4; k++)
                        {
                            if (cube[i][k].value == 0) continue;
                            else
                            {
                                cube[i][j].value = cube[i][k].value;
                                cube[i][j].cube_obj = cube[i][k].cube_obj;
                                cube[i][k].value = 0;
                                lv_myanim_creat(cube[i][j].cube_obj, set_x_cb, 100, 0, 0, -198 * screen_ratio + 131 * k * screen_ratio, -198 * screen_ratio + 131 * j * screen_ratio, 0, move_done_cb);
                                j--; moving_count++; break;
                            }
                        }
                    }
                }
            }
            return;
        }

        /* 向右移动逻辑 */
        if (direction == right)
        {
            for (int32_t i = 0; i < 4; i++)
            {
                for (int32_t j = 3; j > 0; j--)
                {
                    if (cube[i][j].value)
                    {
                        for (int32_t k = j - 1; k >= 0; k--)
                        {
                            if (cube[i][k].value == 0) continue;
                            if (cube[i][k].value != cube[i][j].value) break;
                            if (cube[i][k].value == cube[i][j].value)
                            {
                                cube[i][j].value += cube[i][k].value;
                                cube[i][k].value = 0;
                                lv_myanim_creat(cube[i][k].cube_obj, set_x_cb, 100, 0, 0, -198 * screen_ratio + 131 * k * screen_ratio, -198 * screen_ratio + 131 * j * screen_ratio, &cube[i][j], move_done_cb);
                                moving_count++; break;
                            }
                        }
                    }
                    else
                    {
                        for (int32_t k = j - 1; k >= 0; k--)
                        {
                            if (cube[i][k].value == 0) continue;
                            else
                            {
                                cube[i][j].value = cube[i][k].value;
                                cube[i][j].cube_obj = cube[i][k].cube_obj;
                                cube[i][k].value = 0;
                                lv_myanim_creat(cube[i][j].cube_obj, set_x_cb, 100, 0, 0, -198 * screen_ratio + 131 * k * screen_ratio, -198 * screen_ratio + 131 * j * screen_ratio, 0, move_done_cb);
                                j++; moving_count++; break;
                            }
                        }
                    }
                }
            }
        }
    }
}

/* 创建自定义动画 */
static void lv_myanim_creat(void *var, lv_anim_exec_xcb_t exec_cb, uint32_t time, uint32_t delay, uint32_t playback_time,
                            int32_t start, int32_t end, void *user_data, lv_anim_deleted_cb_t deleted_cb)
{
    lv_anim_t xxx;
    lv_anim_init(&xxx);
    lv_anim_set_var(&xxx, var);
    lv_anim_set_exec_cb(&xxx, exec_cb);
    lv_anim_set_time(&xxx, time);
    lv_anim_set_delay(&xxx, delay);
    lv_anim_set_values(&xxx, start, end);
    if (playback_time) lv_anim_set_playback_time(&xxx, playback_time);
    lv_anim_set_user_data(&xxx, user_data);
    if (deleted_cb) lv_anim_set_deleted_cb(&xxx, deleted_cb);
    lv_anim_start(&xxx);
}

static void set_x_cb(void *var, int32_t v)
{
    lv_obj_set_x(var, v);
}

static void set_y_cb(void *var, int32_t v)
{
    lv_obj_set_y(var, v);
}

static void set_size_cb(void *var, int32_t v)
{
    lv_obj_set_size(var, v, v);
}

/* 移动完成回调 */
static void move_done_cb(lv_anim_t *a)
{
    cube_type_def *xxx = (cube_type_def *)lv_anim_get_user_data(a);

    moving_count--;

    if (xxx)
    {
        lv_obj_set_style_bg_color(xxx->cube_obj, lv_color_hex(color[color_index(xxx->value)]), LV_PART_MAIN);
        lv_label_set_text_fmt(lv_obj_get_child(xxx->cube_obj, 0), "%d", (int)xxx->value);
        if (xxx->value > 4) lv_obj_set_style_text_color(lv_obj_get_child(xxx->cube_obj, 0), lv_color_hex(0xfffffd), LV_PART_MAIN);
        lv_obj_del(a->var);
        lv_myanim_creat(xxx->cube_obj, set_size_cb, 100, 0, 50, 121 * screen_ratio, 131 * screen_ratio, 0, 0);
    }

    if (moving_count == 0)
    {
        int32_t i, j;
        /* 随机生成新方块 */
        do
        {
            i = rand() % 4;
            j = rand() % 4;
        } while (cube[i][j].value != 0);
        new_cube_creat(i, j);

        /* 检查游戏是否结束 */
        if (check_game_over())
        {
            lv_obj_t *game_over_lable = lv_label_create(touch_layer);
            lv_obj_align(game_over_lable, LV_ALIGN_CENTER, 0, -100);
            lv_label_set_text(game_over_lable, "GAME OVER!");
            lv_obj_set_style_text_color(game_over_lable, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_text_font(game_over_lable, &lv_font_montserrat_48, LV_PART_MAIN);
        }
    }
}

/* 创建新方块 */
static void new_cube_creat(int32_t i, int32_t j)
{
    cube[i][j].cube_obj = lv_obj_create(bg);
    lv_obj_align(cube[i][j].cube_obj, LV_ALIGN_CENTER, -198 * screen_ratio + 131 * j * screen_ratio, -198 * screen_ratio + 131 * i * screen_ratio);
    lv_obj_set_size(cube[i][j].cube_obj, 121 * screen_ratio, 121 * screen_ratio);
    lv_obj_clear_flag(cube[i][j].cube_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(cube[i][j].cube_obj, 0, LV_PART_MAIN);
    cube[i][j].value = rand() % 5 == 0 ? 4 : 2;
    lv_myanim_creat(cube[i][j].cube_obj, set_size_cb, 100, 0, 50, 121 * screen_ratio, 131 * screen_ratio, 0, 0);

    lv_obj_set_style_bg_color(cube[i][j].cube_obj, lv_color_hex(color[color_index(cube[i][j].value)]), LV_PART_MAIN);
    lv_obj_t *value_lable = lv_label_create(cube[i][j].cube_obj);
    lv_obj_center(value_lable);
    lv_label_set_text_fmt(value_lable, "%d", (int)cube[i][j].value);
    lv_obj_set_style_text_color(value_lable, lv_color_hex(0x776e65), LV_PART_MAIN);
    lv_obj_set_style_text_font(value_lable, &lv_font_montserrat_48, LV_PART_MAIN);
}

/* 获取颜色索引 */
static int32_t color_index(int32_t num)
{
    int32_t result = 0;
    while (num > 1)
    {
        num >>= 1;
        result++;
    }
    return result;
}

/* 检查游戏是否结束 */
static bool check_game_over(void)
{
    /* 检查是否有空格子 */
    for (int32_t i = 0; i < 4; i++)
    {
        for (int32_t j = 0; j < 4; j++)
        {
            if (cube[i][j].value == 0) return false;
        }
    }

    /* 检查水平方向是否可以合并 */
    for (int32_t i = 0; i < 4; i++)
    {
        for (int32_t j = 0; j < 3; j++)
        {
            if (cube[i][j].value == cube[i][j + 1].value) return false;
        }
    }

    /* 检查垂直方向是否可以合并 */
    for (int32_t i = 0; i < 4; i++)
    {
        for (int32_t j = 0; j < 3; j++)
        {
            if (cube[j][i].value == cube[j + 1][i].value) return false;
        }
    }

    return true;
}