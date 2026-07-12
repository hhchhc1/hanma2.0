#include "lvgl_demo.h"
#include "esp_lvgl_port.h"
#include "esp_lvgl_port_disp.h"
#include "app_smart_home.h"

SemaphoreHandle_t lv_xGuiSemaphore;
lv_indev_data_t touch_data;

/**
 * @brief       读取触摸屏数据
 * @param       indev_drv   : 指向初始化的 'lv_indev_drv_t' 变量的指针
 * @param       data        : 输入设备将数据写入此处
 * @retval      无
 */
void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    assert(indev_drv); /* 确保驱动程序有效 */
    /* 从触摸控制器读取数据到内存 */
    tp_dev.scan(0); /* 扫描触摸数据 */

    if (tp_dev.sta & TP_PRES_DOWN) /* 检查触摸是否按下 */
    {
        data->point.x = tp_dev.x[0]; /* 获取触摸点X坐标 */
        data->point.y = tp_dev.y[0]; /* 获取触摸点Y坐标 */
        data->state = LV_INDEV_STATE_PRESSED; /* 设置状态为按下 */
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED; /* 设置状态为释放 */
    }

    touch_data = *data;
}

/**
 * @brief       初始化输入设备
 * @param       无
 * @retval      无
 */
lv_indev_t * lv_port_indev_init(void)
{
    /* 初始化触摸屏 */
    tp_dev.init();

    /* 初始化输入设备驱动 */
    static lv_indev_drv_t indev_drv;    /* 输入设备驱动的描述符 */
    lv_indev_drv_init(&indev_drv);      /* 基本初始化 */

    /* 触摸板是类似指针的设备 */
    indev_drv.type = LV_INDEV_TYPE_POINTER;

    /* 设置驱动函数 */
    indev_drv.read_cb = touchpad_read;

    /* 最后注册驱动 */
    return lv_indev_drv_register(&indev_drv); /* 注册输入设备驱动 */
}

// static lv_obj_t * chart1;
// static lv_chart_series_t * ser1;
// static lv_chart_series_t * ser2;

// /**
//  * @brief 处理绘制事件的回调函数
//  * 
//  * 本函数用于处理绘制事件，根据事件的不同部分添加相应的绘制效果
//  * 主要功能包括添加渐变区域、线条遮罩和矩形绘制，以及调整分割线的样式
//  * 
//  * @param e 绘制事件的指针，包含事件的相关信息
//  */
// static void draw_event_cb(lv_event_t * e)
// {
//     // 获取事件的目标对象
//     lv_obj_t * obj = lv_event_get_target(e);
//     lv_event_code_t code = lv_event_get_code(e);

//     /*Add the faded area before the lines are drawn*/
//     // 获取绘制部分的描述符
//     lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    
//     // 如果绘制部分是项目区域，则执行以下操作
//     if(dsc->part == LV_PART_ITEMS)
//     {
//         // 如果绘制点不存在，则直接返回
//         if(!dsc->p1 || !dsc->p2) return;

//         // 添加一个线条遮罩，保留线条下方的区域
//         lv_draw_mask_line_param_t line_mask_param;
//         lv_draw_mask_line_points_init(&line_mask_param, dsc->p1->x, dsc->p1->y, dsc->p2->x, dsc->p2->y,
//                                       LV_DRAW_MASK_LINE_SIDE_BOTTOM);
//         int16_t line_mask_id = lv_draw_mask_add(&line_mask_param, NULL);

//         // 添加一个渐变效果：底部透明，顶部覆盖
//         lv_coord_t h = lv_obj_get_height(obj);
//         lv_draw_mask_fade_param_t fade_mask_param;
//         lv_draw_mask_fade_init(&fade_mask_param, &obj->coords, LV_OPA_COVER, obj->coords.y1 + h / 8, LV_OPA_TRANSP,
//                                obj->coords.y2);
//         int16_t fade_mask_id = lv_draw_mask_add(&fade_mask_param, NULL);

//         // 绘制一个矩形，该矩形将受到遮罩的影响
//         lv_draw_rect_dsc_t draw_rect_dsc;
//         lv_draw_rect_dsc_init(&draw_rect_dsc);
//         draw_rect_dsc.bg_opa = LV_OPA_0;
//         draw_rect_dsc.bg_color = dsc->line_dsc->color;

//         lv_area_t a;
//         a.x1 = dsc->p1->x;
//         a.x2 = dsc->p2->x - 1;
//         a.y1 = LV_MIN(dsc->p1->y, dsc->p2->y);
//         a.y2 = obj->coords.y2;
//         lv_draw_rect(dsc->draw_ctx, &draw_rect_dsc, &a);

//         // 移除遮罩
//         lv_draw_mask_free_param(&line_mask_param);
//         lv_draw_mask_free_param(&fade_mask_param);
//         lv_draw_mask_remove_id(line_mask_id);
//         lv_draw_mask_remove_id(fade_mask_id);

//     }
//     else if (dsc->part == LV_PART_INDICATOR)
//     {
//         dsc->radius = 0;
//     }
//     // 处理分割线的绘制
//     else if(dsc->part == LV_PART_MAIN)
//     {
//         lv_draw_line_dsc_t line_dsc;
//         line_dsc.color = lv_color_white();          // 线条颜色
//         line_dsc.width = 1;                       // 线条宽度
//         line_dsc.dash_width = 12;                  // 虚线宽度
//         line_dsc.dash_gap = 2;                    // 虚线间隔
//         line_dsc.opa = LV_OPA_COVER;             // 不透明
//         // line_dsc.blend_mode = LV_BLEND_MODE_NORMAL; // 混合模式为正常
//         line_dsc.round_start = 0;                 // 启用起始端圆角
//         line_dsc.round_end = 0;                   // 启用结束端圆角
//         line_dsc.raw_end = 0;                     // 不禁用端点矩形

//         /* 设置线条的起点和终点 */
//         lv_point_t point1 = {360, 280};
//         lv_point_t point2 = {360, 1000};

//         /* 调用lv_draw_line绘制线条 */
//         lv_draw_line(dsc->draw_ctx, &line_dsc, &point1, &point2);

//         lv_draw_line_dsc_t line_dsc1;
//         line_dsc1.color = lv_color_white();          // 线条颜色
//         line_dsc1.width = 1;                       // 线条宽度
//         line_dsc1.dash_width = 12;                  // 虚线宽度
//         line_dsc1.dash_gap = 2;                    // 虚线间隔
//         line_dsc1.opa = LV_OPA_COVER;             // 不透明
//         // line_dsc1.blend_mode = LV_BLEND_MODE_NORMAL; // 混合模式为正常
//         line_dsc1.round_start = 0;                 // 启用起始端圆角
//         line_dsc1.round_end = 0;                   // 启用结束端圆角
//         line_dsc1.raw_end = 0;                     // 不禁用端点矩形

//         /* 设置线条的起点和终点 */
//         lv_point_t point3 = {0, 640};
//         lv_point_t point4 = {720, 640};

//         /* 调用lv_draw_line绘制线条 */
//         lv_draw_line(dsc->draw_ctx, &line_dsc1, &point3, &point4);

//         // 如果线条描述符或绘制点不存在，则直接返回
//         if(dsc->line_dsc == NULL || dsc->p1 == NULL || dsc->p2 == NULL) return;

//         // 垂直线
//         if(dsc->p1->x == dsc->p2->x)
//         {
//             dsc->line_dsc->color  = lv_palette_lighten(LV_PALETTE_GREY, 1);
//             // 如果dsc的id等于3
//             if(dsc->id == 4) {
//                 // 设置dsc的line_dsc的宽度为2
//                 dsc->line_dsc->width  = 3;
//                 // 设置dsc的line_dsc的dash_gap为0
//                 dsc->line_dsc->dash_gap  = 8;
//                 // 设置dsc的line_dsc的dash_width为0
//                 dsc->line_dsc->dash_width  = 4;
//             }
//             // 否则
//             else {
//                 dsc->line_dsc->width = 2;
//                 dsc->line_dsc->dash_gap  = 12;
//                 dsc->line_dsc->dash_width  = 2;
//             }
//         }
//         // 水平线
//         else
//         {
//             if(dsc->id == 4)
//             {
//                 dsc->line_dsc->width  = 3;
//                 dsc->line_dsc->dash_gap  = 8;
//                 dsc->line_dsc->dash_width  = 4;
//                 // dsc->line_dsc->
//             }
//             else
//             {
//                 dsc->line_dsc->width = 2;
//                 dsc->line_dsc->dash_gap  = 12;
//                 dsc->line_dsc->dash_width  = 2;
//             }

//             dsc->line_dsc->color  = lv_color_white();
//         }
//     }
// }

// static void add_data(lv_timer_t * timer)
// {
//     LV_UNUSED(timer);
//     static uint32_t cnt = 0;
//     lv_chart_set_next_value(chart1, ser1, lv_rand(20, 90));

//     if(cnt % 4 == 0) lv_chart_set_next_value(chart1, ser2, lv_rand(40, 60));

//     cnt++;
// }

// /**
//  * Add a faded area effect to the line chart and make some division lines ticker
//  */
// void lv_example_chart_2(void)
// {
//     /*Create a chart1*/
//     chart1 = lv_chart_create(lv_scr_act());
//     lv_obj_set_size(chart1, lv_obj_get_width(lv_scr_act()), lv_obj_get_width(lv_scr_act()));
//     lv_obj_center(chart1);
//     lv_chart_set_type(chart1, LV_CHART_TYPE_LINE);  /* 线段模式 */
//     lv_obj_set_style_bg_color(chart1,lv_color_black(),LV_STATE_DEFAULT);
//     lv_obj_set_style_radius(chart1, 0, LV_STATE_DEFAULT);                                      /* 无圆角 */
//     lv_obj_set_style_border_opa(chart1, LV_OPA_0, LV_STATE_DEFAULT);                           /* 边界透明 */
//     lv_obj_set_style_outline_width(chart1,0,LV_STATE_DEFAULT);
//     lv_obj_set_style_outline_opa( chart1,LV_OPA_0,LV_STATE_DEFAULT);
//     lv_chart_set_div_line_count(chart1, 10, 10);     /* 设置行列 */

//     lv_obj_add_event_cb(chart1, draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
//     lv_chart_set_update_mode(chart1, LV_CHART_UPDATE_MODE_SHIFT);    /* 旧数据向左移动，并将新数据添加到右侧 */

//     /* 添加两个数据系列 */
//     ser1 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
//     ser2 = lv_chart_add_series(chart1, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_SECONDARY_Y);

//     uint32_t i;
//     for(i = 0; i < 2000; i++)
//     {
//         lv_chart_set_next_value(chart1, ser1, lv_rand(20, 90));
//         lv_chart_set_next_value(chart1, ser2, lv_rand(30, 70));
//     }

//     lv_timer_create(add_data, 200, NULL);
// }

uint8_t wifi_app_flag = 0;

/**
 * @brief       lvgl演示demo入口
 * @param       无
 * @retval      无
 */
void lvgl_demo(void)
{
    /* 初始化LVGL端口配置 */
    lvgl_port_cfg_t lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_cfg.task_stack = 12288; /* 增大栈空间防止 printf 栈溢出 */
    /* 初始化LVGL端口 */
    lvgl_port_init(&lvgl_port_cfg);

    if (lcddev.id == 0x1018 || lcddev.id == 0x7016 || lcddev.id == 0x7084 || lcddev.id == 0X4384 || lcddev.id == 0X4342) /* RGB */
    {
        /* 初始化LVGL显示配置 */
        const lvgl_port_display_cfg_t rgb_disp_cfg = {
            .panel_handle = lcddev.lcd_panel_handle,
            .buffer_size = lcddev.width * lcddev.width,
            .double_buffer = 0,
            .hres = lcddev.width,
            .vres = lcddev.height,
            .monochrome = false,
#if LVGL_VERSION_MAJOR >= 9
            .color_format = LV_COLOR_FORMAT_RGB565,
#endif
            .rotation = {           /* 必须与RGBLCD配置一样 */
                .swap_xy = false,
                .mirror_x = false,
                .mirror_y = false,
            },
            .flags = {
                .buff_dma = false,
                .buff_spiram = false,
                .full_refresh = false,
                .direct_mode = true,
#if LVGL_VERSION_MAJOR >= 9
                .swap_bytes = false,
#endif
            }
        };
        const lvgl_port_display_rgb_cfg_t rgb_cfg = {
            .flags = {
                .bb_mode = true,
                .avoid_tearing = true,
            }
        };

        lvgl_port_add_disp_rgb(&rgb_disp_cfg, &rgb_cfg);
    }
    else
    {
        /* 初始化LVGL显示配置 */
        const lvgl_port_display_cfg_t disp_cfg = {
            .io_handle = lcddev.lcd_dbi_io,         /* 设置io_handle为lcddev.lcd_dbi_io，用于处理显示设备的IO操作 */
            .panel_handle = lcddev.lcd_panel_handle,/* 设置panel_handle为lcddev.lcd_panel_handle，用于处理显示设备的面板操作 */
            .control_handle = NULL,                 /* 设置control_handle为NULL，用于处理显示设备的控制操作 */
            .buffer_size = lcddev.width * lcddev.height,    /* 设置buffer_size为lcddev.height * lcddev.height，用于设置显示设备的缓冲区大小 */
            .double_buffer = false,                 /* 设置double_buffer为false，用于设置显示设备是否使用双缓冲 */
            .hres = lcddev.width,                   /* 设置hres为lcddev.width，用于设置显示设备的水平分辨率 */
            .vres = lcddev.height,                  /* 设置vres为lcddev.height，用于设置显示设备的垂直分辨率 */
            .monochrome = false,                    /* 设置monochrome为false，用于设置显示设备是否为单色 */
            .rotation = {                           /* 旋转值必须与esp_lcd中用于屏幕初始设置的值相同 */
                .swap_xy = false,
                .mirror_x = true,
                .mirror_y = false,
            },
#if LVGL_VERSION_MAJOR >= 9                     /* LVGL9？ */
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
            .color_format = LV_COLOR_FORMAT_RGB888,
#else
            .color_format = LV_COLOR_FORMAT_RGB565,
#endif
#endif
            .flags = {
                .buff_dma = false,                 /* 分配的LVGL缓冲区支持DMA */
                .buff_spiram = true,               /* 分配的LVGL缓冲区使用PSRAM */
#if LVGL_VERSION_MAJOR >= 9
                .swap_bytes = (BSP_LCD_BIGENDIAN ? true : false),
#endif
                .sw_rotate = false,                 /* SW旋转不支持避免撕裂 */
                .full_refresh = true,               /* 全屏刷新 */
                .direct_mode = true,                /* 使用屏幕大小的缓冲区并绘制到绝对坐标 */
            }
        };

        const lvgl_port_display_dsi_cfg_t  dpi_cfg = {
            .flags = {
                .avoid_tearing = true,              /* 开启防撕裂 */
            }
        };

        lvgl_port_add_disp_dsi(&disp_cfg, &dpi_cfg);
    }

    lv_port_indev_init();

    lv_xGuiSemaphore = xSemaphoreCreateBinary(); /* 创建互斥锁 */
    /* 锁定互斥锁，因为LVGL API不是线程安全的 */
    if (lvgl_port_lock(0))
    {
        //lv_app_calculator_init(); //已完成
		if(wifi_app_flag == 1)
		{
			wifi_app_init();
		}
		else
		{
			lv_app_main_ui_init();    //已完成
			app_smart_home_init(lv_app_ui.tile_v[1]);
		}
        //lv_app_music_init();    //已完成
        //lv_app_video_init();
        // lv_demo_benchmark();
        // lv_example_chart_2();
		// app_file_ui_init();
        /* 释放互斥锁 */
        lvgl_port_unlock();  /* 释放互斥锁 */
    }

    while (1)
    {
        lv_ui_del(lv_xGuiSemaphore); //删除子界面
    }
}
