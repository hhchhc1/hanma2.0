/**
 ******************************************************************************
 * @file        app_main_ui.c
 * @version     V1.0
 * @brief       主界面UI
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_main_ui.h"
#include "esp_rtc.h"
#include "app_xiaozhi.h"

// 定义全局变量
grneral_t lv_general;
app_ui_t lv_app_ui;

// 声明图片
LV_IMG_DECLARE(usb);
LV_IMG_DECLARE(brush);
LV_IMG_DECLARE(calculator);
LV_IMG_DECLARE(app_calendar);
LV_IMG_DECLARE(camera);
LV_IMG_DECLARE(l_file);
LV_IMG_DECLARE(music);
LV_IMG_DECLARE(l_wifi);
LV_IMG_DECLARE(video);
LV_IMG_DECLARE(app_2048);
LV_IMG_DECLARE(timer);
LV_IMG_DECLARE(image);
LV_IMG_DECLARE(app);

LV_IMG_DECLARE(Brightness);
LV_IMG_DECLARE(Bluetooth);
LV_IMG_DECLARE(Flight_Mode);
LV_IMG_DECLARE(Mobile_Signal);
LV_IMG_DECLARE(WIFI);
LV_IMG_DECLARE(volumeHigh);

/* 声明字体 */
LV_FONT_DECLARE(Font96_segoe_back)

const lv_img_dsc_t * flexo_img_arr[] = {
    &Bluetooth,
    &Flight_Mode,
    &Mobile_Signal,
    &WIFI,
};

// 定义应用信息数组
const app_info_t app_info[] = {
    {0, "USB", "USB", &usb},
    {1, "brush", "画笔", &brush},
    {2, "calculator", "计算器", &calculator},
    {3, "calendar", "日历", &app_calendar},
    {4, "camera", "摄像头", &camera},
    {5, "file", "文件", &l_file},
    {6, "music", "音乐", &music},
    {7, "app", "应用", &app},
    {8, "video", "视频", &video},
    {9, "2048", "2048", &app_2048},
    {10, "timer", "时间", &timer},
    {11, "image", "图片", &image},
};

// 定义数组元素个数
#define image_mun (int)(sizeof(app_info) / sizeof(app_info[0])) /* 获取数组元素个数 */

lv_obj_t *atk_quick_exit;

/**
 * @brief       Message prompt
 * @param       name : Message
 * @retval      None
 */
void lv_msgbox(char *name)
{
    /* msgbox create */
    atk_quick_exit = lv_msgbox_create(NULL,LV_SYMBOL_WARNING "Notice",name, NULL,true);
    lv_obj_set_size(atk_quick_exit, lv_obj_get_width(lv_scr_act()) - 20, lv_obj_get_width(lv_scr_act()) / 3);   /* set size */
    lv_obj_center(atk_quick_exit);
    lv_obj_set_style_border_width(atk_quick_exit, 0, 0);
    lv_obj_set_style_shadow_width(atk_quick_exit, 20, 0);
    lv_obj_set_style_shadow_color(atk_quick_exit, lv_color_hex(0xa9a9a9),LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(atk_quick_exit,18,LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(atk_quick_exit,20,LV_STATE_DEFAULT);

    lv_obj_t *title = lv_msgbox_get_title(atk_quick_exit);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32,LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(0xff0000),LV_STATE_DEFAULT);

    lv_obj_t *content = lv_msgbox_get_content(atk_quick_exit);
    lv_obj_set_style_text_font(content, &lv_font_montserrat_32,LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(content, lv_color_hex(0x6c6c6c),LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(content, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(content,15,LV_STATE_DEFAULT);

    lv_obj_t * lv_msg_btn = lv_msgbox_get_close_btn(atk_quick_exit);
    lv_obj_set_style_text_font(lv_msg_btn, &lv_font_montserrat_48,LV_STATE_DEFAULT);
    lv_obj_set_size(lv_msg_btn,80,80);

    lv_general.del_parent = atk_quick_exit;
}

void lv_pull_ui(lv_obj_t * parent)
{
    // 动画效果，使父对象的高度变为屏幕高度
    lv_anim_h_act(parent,lv_obj_get_height(lv_scr_act()),false);

    // 创建一个容器对象，作为小界面管理器
    lv_app_ui.app_small_ui.small_cont_manage = lv_obj_create(parent);
    // 设置容器对象的大小
    lv_obj_set_size(lv_app_ui.app_small_ui.small_cont_manage,lv_obj_get_width(lv_scr_act()) / 2 - 20,lv_obj_get_width(lv_scr_act()) / 2 - 20);
    // 设置容器对象的背景颜色
    lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_manage,lv_color_make(86,80,90),LV_STATE_DEFAULT);
    // 设置容器对象的位置
    lv_obj_set_pos(lv_app_ui.app_small_ui.small_cont_manage,0,lv_obj_get_height(lv_scr_act()) / 20);
    // 设置容器对象的圆角
    lv_obj_set_style_radius(lv_app_ui.app_small_ui.small_cont_manage, 20, LV_STATE_DEFAULT);                                      /* 无圆角 */
    // 设置容器对象的边界透明度
    lv_obj_set_style_border_opa(lv_app_ui.app_small_ui.small_cont_manage, LV_OPA_0, LV_STATE_DEFAULT);                           /* 边界透明 */
    // 禁止容器对象的滚动
    lv_obj_clear_flag(lv_app_ui.app_small_ui.small_cont_manage, LV_OBJ_FLAG_SCROLLABLE);                                         /* 禁止滚动 */
    static lv_coord_t  column_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),LV_GRID_TEMPLATE_LAST};   /*列*/
    static lv_coord_t  row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};     /*行*/
    // 设置父对象的列和行描述数组
    lv_obj_set_style_grid_column_dsc_array(lv_app_ui.app_small_ui.small_cont_manage, column_dsc, LV_STATE_DEFAULT);
    lv_obj_set_style_grid_row_dsc_array(lv_app_ui.app_small_ui.small_cont_manage, row_dsc, LV_STATE_DEFAULT);
    // 设置父对象的布局为网格布局
    lv_obj_set_layout(lv_app_ui.app_small_ui.small_cont_manage, LV_LAYOUT_GRID);

    // 循环创建四个按钮
    for (uint8_t i = 0; i < 4; i++)
    {
        // 创建按钮对象
        lv_app_ui.app_small_ui.small_cont_flexo[i] = lv_obj_create(lv_app_ui.app_small_ui.small_cont_manage);
        // 设置按钮的背景颜色
        lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_flexo[i],lv_color_make(50,50,50),LV_STATE_DEFAULT);
        // 设置按钮的圆角
        lv_obj_set_style_radius(lv_app_ui.app_small_ui.small_cont_flexo[i], 100, LV_STATE_DEFAULT);                             /* 无圆角 */
        // 设置按钮的边界透明度
        lv_obj_set_style_border_opa(lv_app_ui.app_small_ui.small_cont_flexo[i], LV_OPA_0, LV_STATE_DEFAULT);                    /* 边界透明 */
        // 设置按钮可点击
        lv_obj_add_flag(lv_app_ui.app_small_ui.small_cont_flexo[i], LV_OBJ_FLAG_CLICKABLE);                                     /* 禁止滚动 */
        // 添加按钮的点击事件回调函数
        lv_obj_add_event_cb(lv_app_ui.app_small_ui.small_cont_flexo[i], lv_menu_interface_event_cb, LV_EVENT_CLICKED, NULL);
        // 设置按钮在网格中的位置
        lv_obj_set_grid_cell(lv_app_ui.app_small_ui.small_cont_flexo[i], LV_GRID_ALIGN_CENTER, i % 2, 1,LV_GRID_ALIGN_CENTER, i / 2, 1);

        // 创建按钮的图标对象
        lv_app_ui.app_small_ui.small_cont_flexo_ico[i] = lv_img_create(lv_app_ui.app_small_ui.small_cont_flexo[i]);
        // 设置图标对象的内容
        lv_img_set_src(lv_app_ui.app_small_ui.small_cont_flexo_ico[i], flexo_img_arr[i]);
        // 设置图标对象的位置
        lv_obj_align(lv_app_ui.app_small_ui.small_cont_flexo_ico[i], LV_ALIGN_CENTER, 0, 0);
        // 设置图标对象的重新着色透明度
        lv_obj_set_style_img_recolor_opa(lv_app_ui.app_small_ui.small_cont_flexo_ico[i], LV_OPA_100, LV_STATE_DEFAULT);
        // 设置图标对象的重新着色颜色
        lv_obj_set_style_img_recolor(lv_app_ui.app_small_ui.small_cont_flexo_ico[i], lv_color_make(255,255,255), LV_STATE_DEFAULT);
    }

    // 创建一个滑动条对象，作为亮度调节器
    lv_app_ui.app_small_ui.small_cont_brightness = lv_slider_create(parent);
    // 设置滑动条对象的大小
    lv_obj_set_size(lv_app_ui.app_small_ui.small_cont_brightness, 150, lv_obj_get_width(lv_scr_act()) / 2 - 20);
    // 设置滑动条对象的范围
    lv_slider_set_range(lv_app_ui.app_small_ui.small_cont_brightness, 30, 100);
    // 设置滑动条对象的位置
    lv_obj_set_pos(lv_app_ui.app_small_ui.small_cont_brightness,lv_obj_get_width(lv_scr_act()) / 2,lv_obj_get_height(lv_scr_act()) / 20);
    // 设置滑动条对象的背景颜色
    lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_brightness,lv_color_make(128,128,128),LV_STATE_DEFAULT);
    // 设置滑动条对象的圆角
    lv_obj_set_style_radius(lv_app_ui.app_small_ui.small_cont_brightness,20,LV_STATE_DEFAULT);
    // 设置滑动条对象的指示器背景颜色
    lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_brightness,lv_color_make(255,255,255),LV_PART_INDICATOR);
    // 设置滑动条对象的指示器圆角
    lv_obj_set_style_radius(lv_app_ui.app_small_ui.small_cont_brightness,20,LV_PART_INDICATOR);
    // 设置滑动条对象的初始值
    lv_slider_set_value(lv_app_ui.app_small_ui.small_cont_brightness,100, LV_ANIM_ON);
    // 设置滑动条对象的旋钮背景透明度
    lv_obj_set_style_bg_opa(lv_app_ui.app_small_ui.small_cont_brightness, LV_OPA_0 ,LV_PART_KNOB);
    // 更新滑动条对象的布局
    lv_obj_update_layout(lv_app_ui.app_small_ui.small_cont_brightness);
    // 添加滑动条对象的点击事件回调函数
    lv_obj_add_event_cb(lv_app_ui.app_small_ui.small_cont_brightness, lv_menu_interface_event_cb, LV_EVENT_PRESSING, NULL);
    // 创建一个图标对象，作为亮度调节器的图标
    lv_obj_t * brightness = lv_img_create(lv_app_ui.app_small_ui.small_cont_brightness);
    // 设置图标对象的内容
    lv_img_set_src(brightness, &Brightness);
    // 设置图标对象的位置
    lv_obj_align(brightness, LV_ALIGN_BOTTOM_MID, 0, -30);

    // 创建一个滑动条对象，作为音量调节器
    lv_app_ui.app_small_ui.small_cont_voice = lv_slider_create(parent);
    // 设置滑动条对象的大小
    lv_obj_set_size(lv_app_ui.app_small_ui.small_cont_voice, 150, lv_obj_get_width(lv_scr_act()) / 2 - 20);
    // 设置滑动条对象的范围
    lv_slider_set_range(lv_app_ui.app_small_ui.small_cont_voice, 0, 100);
    // 设置滑动条对象的位置
    lv_obj_set_pos(lv_app_ui.app_small_ui.small_cont_voice,lv_obj_get_width(lv_scr_act()) / 2 + lv_obj_get_width(lv_app_ui.app_small_ui.small_cont_brightness) + 20,lv_obj_get_height(lv_scr_act()) / 20);
    // 设置滑动条对象的背景颜色
    lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_voice,lv_color_make(128,128,128),LV_STATE_DEFAULT);
    // 设置滑动条对象的圆角
    lv_obj_set_style_radius(lv_app_ui.app_small_ui.small_cont_voice,20,LV_STATE_DEFAULT);
    // 设置滑动条对象的指示器背景颜色
    lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_voice,lv_color_make(255,255,255),LV_PART_INDICATOR);
    // 设置滑动条对象的指示器圆角
    lv_obj_set_style_radius(lv_app_ui.app_small_ui.small_cont_voice,20,LV_PART_INDICATOR);
    // 设置滑动条对象的初始值
    lv_slider_set_value(lv_app_ui.app_small_ui.small_cont_voice,20, LV_ANIM_ON);
    // 设置滑动条对象的旋钮背景透明度
    lv_obj_set_style_bg_opa(lv_app_ui.app_small_ui.small_cont_voice, LV_OPA_0 ,LV_PART_KNOB);
    // 添加滑动条对象的点击事件回调函数
    lv_obj_add_event_cb(lv_app_ui.app_small_ui.small_cont_voice, lv_menu_interface_event_cb, LV_EVENT_PRESSING, NULL);
    // 创建一个图标对象，作为音量调节器的图标
    lv_obj_t * voice = lv_img_create(lv_app_ui.app_small_ui.small_cont_voice);
    // 设置图标对象的内容
    lv_img_set_src(voice, &volumeHigh);
    // 设置图标对象的位置
    lv_obj_align(voice, LV_ALIGN_BOTTOM_MID, 0, -30);
    // 设置图标对象的重新着色透明度
    lv_obj_set_style_img_recolor_opa(voice, LV_OPA_100, LV_STATE_DEFAULT);
    // 设置图标对象的重新着色颜色
    lv_obj_set_style_img_recolor(voice, lv_color_make(0,0,0), LV_STATE_DEFAULT);
}

// 函数名：lv_anim_h_act
// 功能：激活y轴移动动画
// 参数：lv_obt_t * parent，父对象指针
void lv_anim_y_act(lv_obj_t * parent, int32_t v)
{
    /* 不在锁屏状态可点击 */
    for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
    {
        lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
    }
    /* lv_app_ui.app_small_ui.small_cont透明度 */
    lv_obj_set_style_bg_opa(lv_app_ui.app_small_ui.small_cont,LV_OPA_100,LV_STATE_DEFAULT);
}

// 函数名：lv_anim_h_act
// 功能：激活高度动画
// 参数：lv_obt_t * parent，父对象指针
void lv_anim_h_act(lv_obj_t * parent, int32_t v,bool flag)
{
    lv_obj_set_height(parent, v);
    lv_obj_update_layout(parent);

    if (flag == true)
    {
        /* 状态可点击 */
        for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
        {
            lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
        }

        lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont,lv_color_hex(0x000000),LV_STATE_DEFAULT);
    }
}

/**
  * @brief  加载APP图标
  * @param  parent:父类对象
  * @retval 无
  */
void lv_small_app_icon(lv_obj_t *praten)
{
    // 创建SD卡图标
    lv_app_ui.app_small_ui.small_cont_sd = lv_label_create(praten);
    // 设置文本为SD卡图标
    lv_label_set_text(lv_app_ui.app_small_ui.small_cont_sd,LV_SYMBOL_SD_CARD);
    // 设置文本字体
    lv_obj_set_style_text_font(lv_app_ui.app_small_ui.small_cont_sd, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    // 设置文本对齐方式
    lv_obj_align(lv_app_ui.app_small_ui.small_cont_sd, LV_ALIGN_TOP_LEFT, 0, -16);
    // 设置文本颜色
    lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_sd, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);

    // 创建USB图标
    lv_app_ui.app_small_ui.small_cont_usb = lv_label_create(praten);
    // 设置文本为USB图标
    lv_label_set_text(lv_app_ui.app_small_ui.small_cont_usb,LV_SYMBOL_USB);
    // 设置文本字体
    lv_obj_set_style_text_font(lv_app_ui.app_small_ui.small_cont_usb, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    // 设置文本对齐方式
    lv_obj_align_to(lv_app_ui.app_small_ui.small_cont_usb, lv_app_ui.app_small_ui.small_cont_sd,LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    // 设置文本颜色
    lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_usb, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);

    // lv_app_ui.app_small_ui.small_cont_timer = lv_label_create(praten);
    // // 设置文本为日期
    // lv_label_set_text_fmt(lv_app_ui.app_small_ui.small_cont_timer,"%02d:%02d:%02d",calendar.hour, calendar.min, calendar.sec);
    // // 设置文本字体
    // lv_obj_set_style_text_font(lv_app_ui.app_small_ui.small_cont_timer, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    // // 设置文本居中对齐
    // lv_obj_align(lv_app_ui.app_small_ui.small_cont_timer, LV_ALIGN_TOP_MID, 0, 0);
    // // 设置文本颜色
    // lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_timer, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);

    lv_app_ui.app_small_ui.small_cont_power = lv_label_create(praten);
    // 设置文本为电池图标
    lv_label_set_text(lv_app_ui.app_small_ui.small_cont_power,LV_SYMBOL_BATTERY_FULL);
    // 设置文本字体
    lv_obj_set_style_text_font(lv_app_ui.app_small_ui.small_cont_power, &lv_font_montserrat_16, LV_STATE_DEFAULT);
    // 设置文本对齐方式
    lv_obj_align(lv_app_ui.app_small_ui.small_cont_power, LV_ALIGN_TOP_RIGHT, 0, -16);
    // 设置文本颜色
    lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_power, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);

    lv_app_ui.rtc.lv_rtc_timer = lv_timer_create(lv_background_data_processing_timer, 200, NULL); 
}

/**
  * @brief  加载图标
  * @param  parent:父类对象
  * @retval 无
  */
void lv_mid_cont_add_app(lv_obj_t *parent)
{
    // 定义列和行的描述数组 - 改为6列2行
    static lv_coord_t  column_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), 
                                        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                        LV_GRID_TEMPLATE_LAST};   /*列*/
    static lv_coord_t  row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};     /*行*/
    lv_obj_t * cont = lv_obj_create(parent);
    
	lv_obj_set_size(cont, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()) - (lv_obj_get_height(lv_scr_act()) / 20) * 3 - 96);  /* 设置大小 */
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, LV_STATE_DEFAULT);                                  /* 设置背景完全透明 */
    lv_obj_set_style_radius(cont, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
    lv_obj_set_style_border_opa(cont, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
    lv_obj_set_pos(cont, -22, lv_obj_get_height(lv_scr_act()) / 20 + 96);                       /* 设置位置 */
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);                                            /* 禁止滚动 */
    // 设置父对象的列和行描述数组
    lv_obj_set_style_grid_column_dsc_array(cont, column_dsc, LV_STATE_DEFAULT);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, LV_STATE_DEFAULT);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);

    // 遍历所有应用
    for (uint8_t i = 0; i < MAIN_APP_NUM; i++)
    {
        // 创建应用按钮
        lv_app_ui.app_main_ui.app_btn[i] = lv_imgbtn_create(cont);
        // 设置按钮的图标
        lv_imgbtn_set_src(lv_app_ui.app_main_ui.app_btn[i], LV_IMGBTN_STATE_RELEASED, NULL, app_info[i].icon_image,NULL);
        // 设置按钮的大小
        lv_obj_set_size(lv_app_ui.app_main_ui.app_btn[i],usb.header.w,usb.header.h);
        // 添加按钮的事件回调函数
        lv_obj_add_event_cb(lv_app_ui.app_main_ui.app_btn[i], lv_imgbtn_control_event_handler, LV_EVENT_ALL, NULL);
        // 设置按钮的背景透明度
        lv_obj_set_style_bg_opa(lv_app_ui.app_main_ui.app_btn[i],LV_OPA_50,LV_STATE_FOCUSED);
        // 设置按钮在网格中的位置
        lv_obj_set_grid_cell(lv_app_ui.app_main_ui.app_btn[i], LV_GRID_ALIGN_CENTER, i % 6, 1, LV_GRID_ALIGN_CENTER, i / 6, 1);

        // 创建应用名称标签
        lv_app_ui.app_main_ui.app_name[i] = lv_label_create(cont);
        // 设置标签的文本
        lv_label_set_text_fmt(lv_app_ui.app_main_ui.app_name[i], app_info[i].app_text_en);
        // 设置标签的文本颜色
        lv_obj_set_style_text_color(lv_app_ui.app_main_ui.app_name[i],lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);
        // 设置标签的字体
        lv_obj_set_style_text_font(lv_app_ui.app_main_ui.app_name[i], &lv_font_montserrat_24, 0);
        // 设置标签在网格中的位置
        lv_obj_set_grid_cell(lv_app_ui.app_main_ui.app_name[i], LV_GRID_ALIGN_CENTER, i % 6, 1, LV_GRID_ALIGN_END, i / 6, 1);
    }
}

/**
  * @brief  加载box图标
  * @param  parent:父类对象
  * @retval 无
  */
void lv_small_cont_add_app(lv_obj_t *parent)
{
    // 定义一个静态的样式变量style
    static lv_style_t style;
    // 初始化样式变量style
    lv_style_init(&style);
    // 设置样式变量style的布局方式为水平排列
    lv_style_set_flex_flow(&style, LV_FLEX_FLOW_ROW);
    // 设置样式变量style的主轴对齐方式为均匀分布
    lv_style_set_flex_main_place(&style, LV_FLEX_ALIGN_SPACE_AROUND);
    lv_style_set_flex_cross_place(&style, LV_FLEX_ALIGN_SPACE_AROUND);
    // 设置样式变量style的布局方式为弹性布局
    lv_style_set_layout(&style, LV_LAYOUT_FLEX);
    // 将样式变量style应用到父对象parent上
    lv_obj_add_style(parent, &style, 0);

    // 遍历所有应用
    for (uint8_t i = MAIN_APP_NUM; i < MAIN_APP_NUM; i++)
    {
        // 创建应用按钮
        lv_app_ui.app_main_ui.app_btn[i] = lv_imgbtn_create(parent);
        // 设置按钮的图标
        lv_imgbtn_set_src(lv_app_ui.app_main_ui.app_btn[i], LV_IMGBTN_STATE_RELEASED, NULL, app_info[i].icon_image,NULL);
        // 设置按钮的大小
        lv_obj_set_size(lv_app_ui.app_main_ui.app_btn[i],usb.header.w,usb.header.h);
        // 添加按钮的事件回调函数
        lv_obj_add_event_cb(lv_app_ui.app_main_ui.app_btn[i], lv_imgbtn_control_event_handler, LV_EVENT_ALL, NULL);
        // 设置按钮的背景透明度
        lv_obj_set_style_bg_opa(lv_app_ui.app_main_ui.app_btn[i],LV_OPA_50,LV_STATE_FOCUSED);
    }
}

/**
  * @brief  主界面程序入口
  * @param  无
  * @retval 无
  */
static void tileview_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SCROLL_END || code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *tv = lv_event_get_target(e);
        lv_obj_t *act = lv_tileview_get_tile_act(tv);
        for (int i = 0; i < 3; i++) {
            if (lv_app_ui.tile_v[i] == act) {
                lv_app_ui.current_tile = i;
                break;
            }
        }
        for (int i = 0; i < 3; i++) {
            lv_obj_set_style_bg_color(lv_app_ui.page_dots[i],
                (i == lv_app_ui.current_tile) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x666666),
                LV_STATE_DEFAULT);
        }
        /* Show time only on main page (tile 0), hide on Smart Home (tile 1) and AI Chat (tile 2) */
        if (lv_app_ui.app_main_ui.main_time_label) {
            if (lv_app_ui.current_tile != 0) {
                lv_obj_add_flag(lv_app_ui.app_main_ui.main_time_label, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(lv_app_ui.app_main_ui.main_time_label, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void lv_app_main_ui_init(void)
{
    /* 定义字符数组用于显示星期 */
    char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    /* 设置返回控制器置零 */
    lv_general.current_parent = NULL;
    lv_general.del_parent = NULL;
    lv_general.del_function = NULL;
    
    rtc_get_time(); /* 获取当前时间 */

    lv_coord_t scr_w = lv_obj_get_width(lv_scr_act());
    lv_coord_t scr_h = lv_obj_get_height(lv_scr_act());
    lv_coord_t sb_h = scr_h / 20;

    /* ---- TileView: 左右滑动页面切换 ---- */
    lv_app_ui.tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_size(lv_app_ui.tileview, scr_w, scr_h - sb_h);
    lv_obj_set_pos(lv_app_ui.tileview, 0, sb_h);
    lv_obj_set_style_border_width(lv_app_ui.tileview, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(lv_app_ui.tileview, 0, LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(lv_app_ui.tileview, LV_SCROLLBAR_MODE_OFF);

    /* Tile 0 - Main page */
    lv_app_ui.tile_v[0] = lv_tileview_add_tile(lv_app_ui.tileview, 0, 0, LV_DIR_HOR);
    lv_obj_set_style_radius(lv_app_ui.tile_v[0], 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(lv_app_ui.tile_v[0], 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lv_app_ui.tile_v[0], LV_OPA_0, LV_STATE_DEFAULT);

    /* Tile 1 - Smart Home */
    lv_app_ui.tile_v[1] = lv_tileview_add_tile(lv_app_ui.tileview, 1, 0, LV_DIR_HOR);
    lv_obj_set_style_radius(lv_app_ui.tile_v[1], 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(lv_app_ui.tile_v[1], 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lv_app_ui.tile_v[1], LV_OPA_COVER, LV_STATE_DEFAULT);

    /* Tile 2 - AI Chat */
    lv_app_ui.tile_v[2] = lv_tileview_add_tile(lv_app_ui.tileview, 2, 0, LV_DIR_HOR);
    lv_obj_set_style_radius(lv_app_ui.tile_v[2], 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(lv_app_ui.tile_v[2], 0, LV_STATE_DEFAULT);

    /* ---- 页面指示器小圆点 ---- */
    lv_coord_t dot_sz = 10;
    lv_coord_t dot_gap = 16;
    lv_coord_t dots_w = 3 * dot_sz + 2 * dot_gap;
    lv_coord_t dot_y = scr_h - 20;

    for (int i = 0; i < 3; i++) {
        lv_app_ui.page_dots[i] = lv_obj_create(lv_scr_act());
        lv_obj_set_size(lv_app_ui.page_dots[i], dot_sz, dot_sz);
        lv_obj_set_style_radius(lv_app_ui.page_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(lv_app_ui.page_dots[i], 0, 0);
        lv_obj_clear_flag(lv_app_ui.page_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(lv_app_ui.page_dots[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(lv_app_ui.page_dots[i], (scr_w - dots_w) / 2 + i * (dot_sz + dot_gap), dot_y);
    }
    lv_app_ui.current_tile = 0;
    lv_obj_set_style_bg_color(lv_app_ui.page_dots[0], lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(lv_app_ui.page_dots[1], lv_color_hex(0x666666), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(lv_app_ui.page_dots[2], lv_color_hex(0x666666), LV_STATE_DEFAULT);
    lv_obj_move_foreground(lv_app_ui.page_dots[0]);
    lv_obj_move_foreground(lv_app_ui.page_dots[1]);
    lv_obj_move_foreground(lv_app_ui.page_dots[2]);

    /* ---- Main container (inside tile 0) ---- */
    static lv_style_t bg_style;
    lv_style_init(&bg_style);
    lv_style_set_bg_color(&bg_style, lv_color_hex(0x55AB55));               /* 起始颜色（顶部） */
    lv_style_set_bg_grad_color(&bg_style, lv_color_hex(0x55ABDA));          /* 结束颜色（底部） */
    lv_style_set_bg_grad_dir(&bg_style, LV_GRAD_DIR_VER);                   /* 垂直渐变 */
    
    lv_app_ui.main_cont = lv_obj_create(lv_app_ui.tile_v[0]);
    lv_obj_set_size(lv_app_ui.main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(lv_app_ui.main_cont, 0, 0);
    lv_obj_set_style_radius(lv_app_ui.main_cont, 0, LV_STATE_DEFAULT);      /* 无圆角 */
    lv_obj_set_style_border_opa(lv_app_ui.main_cont, LV_OPA_0, LV_STATE_DEFAULT); /* 边界透明 */
    lv_obj_clear_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_SCROLLABLE);         /* 禁止滚动 */
    lv_obj_add_style(lv_app_ui.main_cont, &bg_style, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);                /* 禁止滚动 */

    /* TileView 事件回调 */
    lv_obj_add_event_cb(lv_app_ui.tileview, tileview_event_cb, LV_EVENT_SCROLL_END, NULL);
    lv_obj_add_event_cb(lv_app_ui.tileview, tileview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 在图标上方和状态栏下方之间的区域显示时间
    lv_app_ui.app_main_ui.main_time_label = lv_label_create(lv_scr_act());
    // 设置文本为时间
    lv_label_set_text_fmt(lv_app_ui.app_main_ui.main_time_label, "%02d:%02d", calendar. hour, calendar.min);
    // 设置文本字体为
    lv_obj_set_style_text_font(lv_app_ui.app_main_ui.main_time_label, &Font96_segoe_back, LV_STATE_DEFAULT);
    // 设置文本颜色
    lv_obj_set_style_text_color(lv_app_ui.app_main_ui.main_time_label, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    // 设置文本居中对齐
    lv_obj_align(lv_app_ui.app_main_ui.main_time_label, LV_ALIGN_TOP_MID, 0, (lv_obj_get_height(lv_scr_act()) / 20) * 1.5); 

    // // 创建主界面的图标框容器
    // lv_app_ui.app_main_ui.ico_box_cont = lv_obj_create(lv_app_ui.main_cont);
    // // 设置图标框容器的大小
    // lv_obj_set_size(lv_app_ui.app_main_ui.ico_box_cont, lv_obj_get_width(lv_scr_act()) - 10, lv_obj_get_height(lv_scr_act()) / 5);
    // // 将图标框容器对齐到屏幕底部中间
    // lv_obj_align(lv_app_ui.app_main_ui.ico_box_cont, LV_ALIGN_BOTTOM_MID, 0, 10);
    // // 设置图标框容器的背景颜色
    // lv_obj_set_style_bg_color(lv_app_ui.app_main_ui.ico_box_cont, lv_color_hex(0xe1e4e3), 0);
    // // 设置图标框容器的背景透明度
    // lv_obj_set_style_bg_opa(lv_app_ui.app_main_ui.ico_box_cont, 150, 0);
    // // 设置图标框容器的边框透明度
    // lv_obj_set_style_border_opa(lv_app_ui.app_main_ui.ico_box_cont, 0, 0);
    // // 设置图标框容器的圆角半径
    // lv_obj_set_style_radius(lv_app_ui.app_main_ui.ico_box_cont, 20, 0);

    // 创建小界面的容器
    lv_app_ui.app_small_ui.small_cont = lv_obj_create(lv_layer_sys());
    // 设置小界面的背景颜色
    lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont,lv_color_hex(0x000000),LV_STATE_DEFAULT);
    // 设置小界面的位置
    lv_obj_set_pos(lv_app_ui.app_small_ui.small_cont, 0, 0);
    // 将小界面移到最前面
    lv_obj_move_foreground(lv_app_ui.app_small_ui.small_cont);
    // 设置小界面的圆角半径
    lv_obj_set_style_radius(lv_app_ui.app_small_ui.small_cont, 0, LV_STATE_DEFAULT);
    // 设置小界面的大小
    lv_obj_set_size(lv_app_ui.app_small_ui.small_cont,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()) / 20);
    // 设置小界面的边框透明度
    lv_obj_set_style_border_opa(lv_app_ui.app_small_ui.small_cont,LV_OPA_0,LV_STATE_DEFAULT); // 设置小窗口的边框透明度为0
    lv_obj_clear_flag(lv_app_ui.app_small_ui.small_cont, LV_OBJ_FLAG_SCROLLABLE);               /* 禁止滚动 */

    lv_small_app_icon(lv_app_ui.app_small_ui.small_cont);       // 在顶部窗口中添加应用图标
    lv_mid_cont_add_app(lv_app_ui.main_cont);                   // 在主窗口中添加应用
    // lv_small_cont_add_app(lv_app_ui.app_main_ui.ico_box_cont);  // 在小窗口中添加应用

    // lv_app_ui.lock_screen.lock_screen_cont = lv_img_create(lv_scr_act());
    // lv_obj_set_size(lv_app_ui.lock_screen.lock_screen_cont, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));
    // lv_img_set_src(lv_app_ui.lock_screen.lock_screen_cont,&bg);

    // lv_app_ui.lock_screen.lock_screen_date = lv_label_create(lv_app_ui.lock_screen.lock_screen_cont);
    // // 设置文本为日期
    // lv_label_set_text_fmt(lv_app_ui.lock_screen.lock_screen_date,"%02d:%02d:%02d", calendar.hour, calendar.min, calendar.sec);
    // // 设置文本字体
    // lv_obj_set_style_text_font(lv_app_ui.lock_screen.lock_screen_date, &lv_font_montserrat_48, LV_STATE_DEFAULT);
    // lv_obj_align(lv_app_ui.lock_screen.lock_screen_date, LV_ALIGN_LEFT_MID, 20, 50);
    // // 设置文本颜色
    // lv_obj_set_style_text_color(lv_app_ui.lock_screen.lock_screen_date, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);

    // lv_app_ui.lock_screen.lock_screen_time = lv_label_create(lv_app_ui.lock_screen.lock_screen_cont);
    // // 设置文本为日期
    // lv_label_set_text_fmt(lv_app_ui.lock_screen.lock_screen_time,"%04d-%02d-%02d",calendar.year, calendar.month, calendar.date);
    // // 设置文本字体
    // lv_obj_set_style_text_font(lv_app_ui.lock_screen.lock_screen_time, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    // lv_obj_align_to(lv_app_ui.lock_screen.lock_screen_time,lv_app_ui.lock_screen.lock_screen_date, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    // // 设置文本颜色
    // lv_obj_set_style_text_color(lv_app_ui.lock_screen.lock_screen_time, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);

    // lv_app_ui.lock_screen.lock_screen_week = lv_label_create(lv_app_ui.lock_screen.lock_screen_cont);
    // // 设置文本为日期
    // lv_label_set_text_fmt(lv_app_ui.lock_screen.lock_screen_week,"%s",weekdays[calendar.week]);
    // // 设置文本字体
    // lv_obj_set_style_text_font(lv_app_ui.lock_screen.lock_screen_week, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    // lv_obj_align_to(lv_app_ui.lock_screen.lock_screen_week,lv_app_ui.lock_screen.lock_screen_date, LV_ALIGN_OUT_RIGHT_MID, 10, 20);
    // // 设置文本颜色
    // lv_obj_set_style_text_color(lv_app_ui.lock_screen.lock_screen_week, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);

    // /* lv_app_ui.app_small_ui.small_cont透明度 */
    // lv_obj_set_style_bg_opa(lv_app_ui.app_small_ui.small_cont,LV_OPA_20,LV_STATE_DEFAULT);
    // /* 锁屏状态不可点击 */
    // for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
    // {
    //     lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
    // }
	
	/* 小界面完全显示 */
    lv_obj_set_style_bg_opa(lv_app_ui.app_small_ui.small_cont,LV_OPA_100,LV_STATE_DEFAULT);

    lv_obj_add_event_cb(lv_scr_act(), lv_scr_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(lv_layer_sys(), lv_scr_event_cb, LV_EVENT_ALL, NULL);

    /* Initialize AI Chat UI in Tile 2 */
    app_xiaozhi_init(lv_app_ui.tile_v[2]);
}