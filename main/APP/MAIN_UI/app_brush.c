/**
 ******************************************************************************
 * @file        app_brush.c
 * @version     V1.0
 * @brief       Brush APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_brush.h"

touch_ui_t lv_touch;
uint16_t width = 320;
uint16_t height = 240;

/**
 * @brief       触摸屏定时器任务
 * @param       task：定时器
 * @retval      无
 */
void lv_touch_task(lv_timer_t *task)
{
    if (touch_data.state == LV_INDEV_STATE_PR)  // 触摸点按下
    {
        // 将触摸点转换为相对于画布的坐标
        int x = touch_data.point.x - 0;
        int y = touch_data.point.y - 64;
        // 获取画布的宽度和高度
        int canvas_width = width;
        int canvas_height = height;
        /* 判断触摸点是否在画布的有效区域内 */
        if (touch_data.point.x >= x && touch_data.point.x < (x + canvas_width) && 
            touch_data.point.y >= y && touch_data.point.y < (y + canvas_height))
        {
            if (lv_touch.last_point.x == -1 || lv_touch.last_point.y == -1)
            {
                // 初始化 last_point 为触摸的第一个点
                lv_touch.last_point.x = x;
                lv_touch.last_point.y = y;
            }

            lv_touch.current_point.x = x;  // 更新当前触摸点
            lv_touch.current_point.y = y;

            // 设置线条描述符
            lv_draw_line_dsc_t line_dsc1;
            line_dsc1.color = lv_touch.pen_color;       // 线条颜色为红色
            line_dsc1.width = lv_touch.touch_pen_size;  // 线条宽度
            line_dsc1.opa = LV_OPA_COVER;               // 不透明
            line_dsc1.round_start = 1;                  // 起始端不圆角
            line_dsc1.round_end = 1;                    // 结束端不圆角

            // 使用画布绘制线条
            lv_canvas_draw_line(lv_touch.lv_touch_cont, (lv_point_t[]) {lv_touch.last_point, lv_touch.current_point}, 2, &line_dsc1);
            lv_obj_invalidate(lv_touch.lv_touch_cont);
            lv_timer_handler();  // 处理 LVGL 事件
            // 更新 last_point 为当前点
            lv_touch.last_point = lv_touch.current_point;
        }
    }
    else
    {
        lv_touch.last_point.x = -1;  // 松开时重置 last_point
        lv_touch.last_point.y = -1;
    }
}
static uint8_t box_pen_state = 0;
static uint8_t box_pen_color_state = 0;

/**
 * @brief  Music playback event callback
 * @param  *e ：event
 * @return None
 */
static void touch_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);


    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_remove_event_cb(lv_scr_act(),lv_scr_event_cb);
        if (target == lv_touch.box_label_eraser)
        {
           lv_canvas_fill_bg(lv_touch.lv_touch_cont, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
        }
        else if (target == lv_touch.box_label_pen)
        {
            if (box_pen_state == 0)
            {
                lv_obj_clear_flag(lv_touch.box_slider, LV_OBJ_FLAG_HIDDEN);        /* 清除对象标志 */
                lv_obj_clear_flag(lv_touch.box_slider_label, LV_OBJ_FLAG_HIDDEN);  /* 清除对象标志 */
                lv_obj_clear_flag(lv_touch.box_sound_label, LV_OBJ_FLAG_HIDDEN);   /* 清除对象标志 */
                box_pen_state = 1;
            }
        }
        else if (target == lv_touch.box_label_color)
        {
            if (box_pen_color_state == 0)
            {
                lv_obj_clear_flag(lv_touch.box_colorwheel, LV_OBJ_FLAG_HIDDEN);   /* 清除对象标志 */
                box_pen_color_state = 1;
            }
        }
    }
    else if (code == LV_EVENT_VALUE_CHANGED)
    {
        if (target == lv_touch.box_slider)
        {
            lv_label_set_text_fmt(lv_touch.box_slider_label, "%d",(int)lv_slider_get_value(target));
        }
    }
    else if (code == LV_EVENT_RELEASED)
    {
        if (target == lv_touch.box_slider)
        {
            lv_touch.touch_pen_size = lv_slider_get_value(target);           /* 获取滑块当前值 */
            lv_obj_add_flag(lv_touch.box_slider, LV_OBJ_FLAG_HIDDEN);        /* 添加对象标志 */
            lv_obj_add_flag(lv_touch.box_slider_label, LV_OBJ_FLAG_HIDDEN);  /* 添加对象标志 */
            lv_obj_add_flag(lv_touch.box_sound_label, LV_OBJ_FLAG_HIDDEN);   /* 添加对象标志 */
            box_pen_state = 0;
        }
        else if (target == lv_touch.box_colorwheel)
        {
            lv_touch.pen_color = lv_colorwheel_get_rgb(target);  
            lv_obj_add_flag(lv_touch.box_colorwheel, LV_OBJ_FLAG_HIDDEN);   /* 添加对象标志 */
            box_pen_color_state = 0; 
        }
		
		lv_obj_add_event_cb(lv_scr_act(), lv_scr_event_cb, LV_EVENT_ALL, NULL);
    }
}

void lv_brush_del(void)
{
    lv_timer_del(lv_touch.touch_timer);
    free(lv_touch.cbuf);
    lv_touch.cbuf = NULL;
    lv_touch.lv_touch_cont = NULL;
    box_pen_state = 0;
    box_pen_color_state = 0;
}

/**
 * @brief       初始化手绘板并创建定时器任务
 * @param       无
 * @retval      无
 */
void lv_app_brush_init(void)
{
    lv_touch.touch_pen_size = 10;
    lv_touch.pen_color = _LV_COLOR_MAKE_TYPE_HELPER LV_COLOR_MAKE(255, 0, 0);
    width = lv_obj_get_width(lv_scr_act());
    height = lv_obj_get_height(lv_scr_act()) / 2 + 100;  /* 高度调整为+100 */

    lv_touch.box_eraser_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(lv_touch.box_eraser_cont, lv_color_hex(0x000000), LV_STATE_DEFAULT);                  /* 设置背景颜色 */
    lv_obj_set_size(lv_touch.box_eraser_cont, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()) - lv_obj_get_height(lv_scr_act()) / 20);  /* 设置容器大小 - 调整为15 */
    lv_obj_set_style_radius(lv_touch.box_eraser_cont, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
    lv_obj_set_style_border_opa(lv_touch.box_eraser_cont, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
    lv_obj_set_pos(lv_touch.box_eraser_cont, 0, lv_obj_get_height(lv_scr_act()) / 20);                              /* 设置位置 - 调整为15 */
    lv_obj_clear_flag(lv_touch.box_eraser_cont, LV_OBJ_FLAG_SCROLLABLE);                                            /* 禁止滚动 */
    lv_obj_update_layout(lv_touch.box_eraser_cont);
    // 为画布申请缓冲区内存
    // lv_touch.cbuf = heap_caps_malloc(width * height * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    ESP_ERROR_CHECK(esp_dma_malloc(width * height * sizeof(lv_color_t), ESP_DMA_MALLOC_FLAG_PSRAM, (void *)&lv_touch.cbuf, NULL));

    if (lv_touch.cbuf == NULL)
    {
        ESP_LOGW("TAG", "cbuf malloc failed");
        return;
    }

    // 创建画布
    lv_touch.lv_touch_cont = lv_canvas_create(lv_touch.box_eraser_cont);
    lv_obj_align(lv_touch.lv_touch_cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_canvas_set_buffer(lv_touch.lv_touch_cont, lv_touch.cbuf, width, height, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(lv_touch.lv_touch_cont, lv_color_hex(0xFFFFFF), LV_OPA_COVER);
    lv_obj_add_flag(lv_touch.lv_touch_cont, LV_OBJ_FLAG_CLICKABLE); /* 可点击 */

    lv_touch.box_cont = lv_obj_create(lv_touch.lv_touch_cont);
    lv_obj_set_size(lv_touch.box_cont, 300, 80);  /* 调整为300x80 */
    lv_obj_align(lv_touch.box_cont,LV_ALIGN_TOP_MID,0,0);
    lv_obj_set_style_bg_color(lv_touch.box_cont,lv_color_make(68,72,75),0);
    lv_obj_set_style_radius(lv_touch.box_cont, 0, LV_STATE_DEFAULT);             /* 无圆角 */
    lv_obj_set_style_border_opa(lv_touch.box_cont, LV_OPA_0, LV_STATE_DEFAULT);  /* 边界透明 */

    lv_touch.box_label_pen = lv_label_create(lv_touch.box_cont);
    lv_obj_set_ext_click_area(lv_touch.box_label_pen,80);  /* 调整为80 */
    lv_obj_set_style_text_font(lv_touch.box_label_pen, &lv_font_montserrat_24,LV_STATE_DEFAULT);  /* 字体调整为24 */
    lv_label_set_text(lv_touch.box_label_pen, LV_SYMBOL_EDIT);
    lv_obj_align(lv_touch.box_label_pen,LV_ALIGN_LEFT_MID,15,0);  /* 边距调整为15 */
    lv_obj_set_style_bg_color(lv_touch.box_label_pen,lv_color_hex(0xFFFFFF),LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(lv_touch.box_label_pen,LV_OPA_20,LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(lv_touch.box_label_pen,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
    lv_obj_add_flag(lv_touch.box_label_pen, LV_OBJ_FLAG_CLICKABLE); // 添加可点击标志
    /* 添加事件 */
    lv_obj_add_event_cb(lv_touch.box_label_pen, touch_event_cb, LV_EVENT_ALL, NULL);

    lv_touch.box_label_eraser = lv_label_create(lv_touch.box_cont);
    lv_obj_set_ext_click_area(lv_touch.box_label_eraser,80);  /* 调整为80 */
    lv_obj_set_style_text_font(lv_touch.box_label_eraser, &lv_font_montserrat_24,LV_STATE_DEFAULT);  /* 字体调整为24 */
    lv_label_set_text(lv_touch.box_label_eraser, LV_SYMBOL_DRIVE);
    lv_obj_align(lv_touch.box_label_eraser,LV_ALIGN_CENTER,0,0);
    lv_obj_set_style_bg_color(lv_touch.box_label_eraser,lv_color_hex(0xFFFFFF),LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(lv_touch.box_label_eraser,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lv_touch.box_label_eraser,LV_OPA_20,LV_STATE_FOCUSED);
    lv_obj_add_flag(lv_touch.box_label_eraser, LV_OBJ_FLAG_CLICKABLE); // 添加可点击标志
    /* 添加事件 */
    lv_obj_add_event_cb(lv_touch.box_label_eraser, touch_event_cb, LV_EVENT_ALL, NULL);

    lv_touch.box_label_color = lv_label_create(lv_touch.box_cont);
    lv_obj_set_ext_click_area(lv_touch.box_label_color,80);  /* 调整为80 */
    lv_obj_set_style_text_font(lv_touch.box_label_color, &lv_font_montserrat_24,LV_STATE_DEFAULT);  /* 字体调整为24 */
    lv_label_set_text(lv_touch.box_label_color, LV_SYMBOL_REFRESH);
    lv_obj_align(lv_touch.box_label_color,LV_ALIGN_RIGHT_MID,-15,0);  /* 边距调整为15 */
    lv_obj_set_style_bg_color(lv_touch.box_label_color,lv_color_hex(0xFFFFFF),LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(lv_touch.box_label_color,LV_OPA_20,LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(lv_touch.box_label_color,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
    lv_obj_add_flag(lv_touch.box_label_color, LV_OBJ_FLAG_CLICKABLE); // 添加可点击标志
    /* 添加事件 */
    lv_obj_add_event_cb(lv_touch.box_label_color, touch_event_cb, LV_EVENT_ALL, NULL);


    lv_touch.box_colorwheel = lv_colorwheel_create(lv_touch.box_eraser_cont, true);
    lv_obj_set_size(lv_touch.box_colorwheel, 130, 130);  /* 调整为150x150 */
    lv_obj_align_to(lv_touch.box_colorwheel, lv_touch.lv_touch_cont,LV_ALIGN_OUT_BOTTOM_MID, 0, 10);  /* 间距调整为30 */
    /* 添加事件 */
    lv_obj_add_event_cb(lv_touch.box_colorwheel, touch_event_cb, LV_EVENT_ALL, NULL);

    lv_touch.box_slider = lv_slider_create(lv_touch.box_eraser_cont); /* 创建滑块 */
    lv_slider_set_range(lv_touch.box_slider,10,20);
    lv_obj_set_size(lv_touch.box_slider,lv_obj_get_width(lv_scr_act()) / 2, 20); /* 设置大小 - 高度调整为20 */
    lv_obj_align_to(lv_touch.box_slider, lv_touch.lv_touch_cont,LV_ALIGN_OUT_BOTTOM_MID, 0, 75);  /* 间距调整为75 */
    lv_slider_set_value(lv_touch.box_slider, 2, LV_ANIM_ON); /* 设置当前值 */
    /* 添加事件 */
    lv_obj_add_event_cb(lv_touch.box_slider, touch_event_cb, LV_EVENT_ALL, NULL);

    /* 百分比标签 */
    lv_touch.box_slider_label = lv_label_create(lv_touch.box_eraser_cont); /* 创建百分比标签 */
    lv_label_set_text_fmt(lv_touch.box_slider_label, "%d",lv_touch.touch_pen_size); /* 设置文本内容 */
    /* 设置字体 */
    lv_obj_set_style_text_font(lv_touch.box_slider_label, &lv_font_montserrat_20, LV_STATE_DEFAULT);  /* 字体调整为20 */
    lv_obj_set_style_text_color(lv_touch.box_slider_label, lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    /* 设置位置 */
    lv_obj_align_to(lv_touch.box_slider_label, lv_touch.box_slider, LV_ALIGN_OUT_RIGHT_MID, 15, 0);  /* 间距调整为15 */

    /* 音量图标 */
    lv_touch.box_sound_label = lv_label_create(lv_touch.box_eraser_cont); /* 创建音量标签 */
    /* 设置文本内容：音量图标 */
    lv_label_set_text(lv_touch.box_sound_label, LV_SYMBOL_EDIT);
    /* 设置字体 */
    lv_obj_set_style_text_font(lv_touch.box_sound_label, &lv_font_montserrat_20, LV_STATE_DEFAULT);  /* 字体调整为20 */
    lv_obj_set_style_text_color(lv_touch.box_sound_label, lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    /* 设置位置 */
    lv_obj_align_to(lv_touch.box_sound_label, lv_touch.box_slider, LV_ALIGN_OUT_LEFT_MID, -15, 0);  /* 间距调整为15 */

    lv_obj_add_flag(lv_touch.box_colorwheel, LV_OBJ_FLAG_HIDDEN);    /* 清除对象标志 */
    lv_obj_add_flag(lv_touch.box_slider, LV_OBJ_FLAG_HIDDEN);        /* 清除对象标志 */
    lv_obj_add_flag(lv_touch.box_slider_label, LV_OBJ_FLAG_HIDDEN);  /* 清除对象标志 */
    lv_obj_add_flag(lv_touch.box_sound_label, LV_OBJ_FLAG_HIDDEN);   /* 清除对象标志 */

    // 创建触摸屏定时器任务
    lv_touch.touch_timer = lv_timer_create(lv_touch_task, 1, NULL);

    lv_general.current_parent = lv_touch.box_eraser_cont;
    lv_general.del_function = lv_brush_del;
}
