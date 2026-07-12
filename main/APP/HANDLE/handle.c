#include "handle.h"

// 定义就绪列表
unsigned int  app_readly_list[32];                              /* 前导置零 */
/* 菜单图标就绪列表 */
unsigned int  menu_readly_list[10];                             /* 前导置零 */
// 定义触发标志
uint8_t lv_trigger_bit = 0;                                     /* 触发标志 */
// 定义加载索引
uint8_t load_index = 0;                                         /* 加载索引 */

SemaphoreHandle_t lv_xGuiSemaphore_handle;

static bool is_blue[4] = {false, false, false, false};          /* 默认是灰色 */

// 判断是否在2048应用中
static bool is_in_2048_app = false;

/**
  * @brief  前导置零
  * @param  app_readly_list:就绪列表
  * @retval 返回APP按下的数值
  */
int lv_clz(unsigned int  app_readly_list[])
{
    int bit = 0;

    for (int i = 0; i < 32; i++)
    {
        if (app_readly_list[i] == 1)
        {
            break;
        }

        bit ++ ;
    }

    return bit;
}

/**************************** 用户处理接口 ********************************/
/**
  * @brief  菜单界面处理
  * @param  event : 事件
  * @retval 无
  */
void lv_menu_interface_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t * obj = lv_event_get_target(event);

    if (code == LV_EVENT_PRESSING)
    {
        if (obj == lv_app_ui.app_small_ui.small_cont_brightness)
        {
            /* 控制屏幕亮度操作 */
            bsp_display_brightness_set((int)lv_slider_get_value(obj));
        }
        else if (obj == lv_app_ui.app_small_ui.small_cont_voice)
        {
            /* 设置ES8311音量 */
			esp_codec_dev_set_out_vol(codec_handle, (int)lv_slider_get_value(obj));
        }
    }
    else if (code == LV_EVENT_CLICKED)
    {
        for (uint8_t i = 0;i < 4;i ++)
        {
            // 判断点击的是否是当前按键
            if (obj == lv_app_ui.app_small_ui.small_cont_flexo[i])
            {
                if (is_blue[i])
                {
                    // 如果当前按键是蓝色，点击后变为灰色
                    lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_flexo[i], lv_color_make(50,50,50), LV_STATE_DEFAULT);  // 灰色
                }
                else
                {
                    if (obj == lv_app_ui.app_small_ui.small_cont_flexo[2])
                    {
                        // 如果当前按键是灰色，点击后变为绿色
                        lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_flexo[i], lv_color_make(101, 196, 102), LV_STATE_DEFAULT);  // 蓝色
                    }
                    else
                    {
                        // 如果当前按键是灰色，点击后变为蓝色
                        lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_flexo[i], lv_color_make(52, 120, 245), LV_STATE_DEFAULT);  // 蓝色
                    }
                }

                // 切换当前按键的颜色状态
                is_blue[i] = !is_blue[i];
            }

            if (is_blue[i] == true && obj == lv_app_ui.app_small_ui.small_cont_flexo[i])
            {
                menu_readly_list[i] = 1;

                lv_trigger_bit = ((unsigned int)lv_clz((menu_readly_list)));
                menu_readly_list[lv_trigger_bit] = 0;
                printf("开启menu_trigger_bit:%d\n",lv_trigger_bit);
                switch(lv_trigger_bit)
                {
                    case 0:
                        /* 开启蓝牙操作 */
                        break;

                    case 1:
                        /* 开启飞行模式操作 */
                        break;

                    case 2:
                        /* 开启移动模式操作 */
                        break;

                    case 3:
                        /* 开启wifi操作 */
                        break;
                }
            }
            else if (is_blue[i] == false && obj == lv_app_ui.app_small_ui.small_cont_flexo[i])
            {
                menu_readly_list[i] = 1;

                lv_trigger_bit = ((unsigned int)lv_clz((menu_readly_list)));
                menu_readly_list[lv_trigger_bit] = 0;
                printf("关闭menu_trigger_bit:%d\n",lv_trigger_bit);
                switch(lv_trigger_bit)
                {
                    case 0:
                        /* 关闭蓝牙操作 */
                        break;

                    case 1:
                        /* 关闭飞行模式操作 */
                        break;

                    case 2:
                        /* 关闭移动模式操作 */
                        break;

                    case 3:
                        /* 关闭wifi操作 */
                        break;
                }
            }
        }
    }
}


/**
  * @brief  主界面的APP图标处理
  * @param  event : 事件
  * @retval 无
  */
void lv_imgbtn_control_event_handler(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t * obj = lv_event_get_target(event);

    if (code == LV_EVENT_CLICKED)
    {

        for (int i = 0;i < MAIN_APP_NUM;i ++)
        {
            if (obj == lv_app_ui.app_main_ui.app_btn[i])
            {
                app_readly_list[i] = 1 ;
            }
        }

        lv_trigger_bit = ((unsigned int)lv_clz((app_readly_list)));
        app_readly_list[lv_trigger_bit] = 0;
        printf("lv_trigger_bit:%d\n",lv_trigger_bit);
        switch(lv_trigger_bit)
        {
            case 0:
			    lv_app_usb_otg_init();
                is_in_2048_app = false;  
                break;

            case 1:
                lv_obj_add_flag(lv_app_ui.main_cont,LV_OBJ_FLAG_HIDDEN);
                for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
                {
                    lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
                }
                lv_app_brush_init();
                is_in_2048_app = false;  
                break;

            case 2:
                lv_obj_add_flag(lv_app_ui.main_cont,LV_OBJ_FLAG_HIDDEN);
                for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
                {
                    lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
                }
                lv_app_calculator_init();
                is_in_2048_app = false;  
                break;

            case 3:
                lv_obj_add_flag(lv_app_ui.main_cont,LV_OBJ_FLAG_HIDDEN);
                for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
                {
                    lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
                }
                app_calendar_ui_init();
                is_in_2048_app = false;  
                break;
            case 4:
				app_camera_ui_init();
                is_in_2048_app = false;  
                break;

            case 5:
				app_file_ui_init();
                is_in_2048_app = false; 
                break;

            case 6:
                lv_app_music_init();
                is_in_2048_app = false;  
                break;

            case 7:
                // lv_msgbox("Unfulfilled");
                is_in_2048_app = false; 
                break;

            case 8:
                lv_app_video_init();
                is_in_2048_app = false;  
                break;
            case 9:
			    app_2048_ui_init();
                is_in_2048_app = true;  
                break;
            case 10:
			    lv_app_timer_init();
                is_in_2048_app = false; 
                break;
            case 11:
                lv_app_pic_init();
                is_in_2048_app = false; 
                break;
            default:
                break;
        }
    }
}

/* 下拉菜单状态枚举 */
typedef enum {
    PULL_MENU_CLOSED = 0,     /* 下拉菜单关闭 */
    PULL_MENU_OPENED = 1,     /* 下拉菜单打开 */
} pull_menu_state_t;

static pull_menu_state_t pull_menu_state = PULL_MENU_CLOSED;  /* 下拉菜单状态 */

/**
  * @brief  屏幕事件回调函数
  * @param  event : 事件
  * @retval 无
  */
void lv_scr_event_cb(lv_event_t *event)
{
    // 获取事件代码
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t * obj = lv_event_get_target(event);
    // 如果事件代码为LV_EVENT_GESTURE
    if (code == LV_EVENT_GESTURE)
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

        if (dir == LV_DIR_BOTTOM && pull_menu_state == PULL_MENU_CLOSED && lv_touch.lv_touch_cont == NULL)
        {
            // 在2048应用中禁用下滑显示菜单
            if (!is_in_2048_app) {
                /* 状态栏变为黑色背景 */
                lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont,lv_color_hex(0x000000),LV_STATE_DEFAULT);
                
                /* 主界面按钮不可点击 */
                for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
                {
                    lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
                }

                lv_pull_ui(lv_app_ui.app_small_ui.small_cont);
                pull_menu_state = PULL_MENU_OPENED;
            }
        }
        /* 上拉关闭菜单或返回主界面 */
        else if (dir == LV_DIR_TOP)
        {
            /* 如果在Smart Home页面，返回主页 */
            if (lv_app_ui.tileview != NULL && lv_app_ui.current_tile == 1)
            {
                lv_obj_set_tile_id(lv_app_ui.tileview, 0, 0, LV_ANIM_ON);
            }
            /* 如果下拉菜单是打开状态，则关闭它 */
            else if (pull_menu_state == PULL_MENU_OPENED)
            {     
                lv_anim_h_act(lv_app_ui.app_small_ui.small_cont, lv_obj_get_height(lv_scr_act()) / 20, true);
                pull_menu_state = PULL_MENU_CLOSED;
                
                for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
                {
                    lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
                }
            }
            /* 如果是在子应用中，返回主界面（在2048 APP中禁用） */
            else if (lv_general.current_parent != NULL && !is_in_2048_app)
            {
                /* 如果是相机应用，调用专门的退出函数 */ 
                if (lv_general.current_parent == lv_camera_ui.camera_main_ui) 
				{
                    app_camera_exit();
                } 
				else 
				{
                    lv_obj_clear_flag(lv_app_ui.main_cont,LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(lv_general.current_parent,LV_OBJ_FLAG_HIDDEN);
                    xSemaphoreGive(lv_xGuiSemaphore_handle);
                }
            }
        }
        else if (dir == LV_DIR_LEFT)
        {
            if (lv_pic_ui.pic_main_ui != NULL)
            {
                xSemaphoreGive(xSemaphore_next);
            }
            /* TileView 原生处理左右滑动切换页面 */
        }
        else if (dir == LV_DIR_RIGHT)
        {
            if (lv_pic_ui.pic_main_ui != NULL)
            {
                xSemaphoreGive(xSemaphore_prev);
            }
            /* TileView 原生处理左右滑动切换页面 */
        }
    }
    else if (code == LV_EVENT_RELEASED)
    {
        for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
        {
            lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
        }
    }

    
}

/* 界面退出后删除子界面 */
void lv_ui_del(SemaphoreHandle_t BinarySemaphore)
{
    lv_xGuiSemaphore_handle = BinarySemaphore;
    xSemaphoreTake(BinarySemaphore, portMAX_DELAY);     /* 同步删除子界面信号量 */
    
    /* 锁定互斥锁，因为LVGL API不是线程安全的 */
    if (lvgl_port_lock(0))
    {
        lv_obj_del(lv_general.current_parent);

        if (lv_general.del_function != NULL)
        {
            lv_general.del_function();
            lv_general.del_function = NULL;
        }
        lv_general.current_parent = NULL;
        lv_obj_update_layout(lv_scr_act());
        
        /* 当子界面被删除时，重置2048标志 */
        is_in_2048_app = false;
        
        /* 释放互斥锁 */
        lvgl_port_unlock();  /* 释放互斥锁 */
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    lv_event_send(lv_scr_act(), LV_EVENT_RELEASED, NULL); /* 发送事件，刷新界面 */
}

/**************************** 板载实现接口 ********************************/
esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100)
    {
        brightness_percent = 100;
    }
    if (brightness_percent < 0)
    {
        brightness_percent = 0;
    }

    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    return ESP_OK;
}

/**************************** 后台数据处理定时器 ********************************/

/* 定义字符数组用于显示星期 */
char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

/**
  * @brief  后台数据处理定时器
  * @param  None
  * @retval None
  */
void lv_background_data_processing_timer(lv_timer_t* timer)
{
	uint8_t key;

    rtc_get_time(); // 获取当前时间

	// 更新时间显示（只显示小时和分钟）
	if (lv_general.current_parent != lv_camera_ui.camera_main_ui) 
	{
		if(lv_app_ui.app_main_ui.main_time_label) {
           lv_label_set_text_fmt(lv_app_ui.app_main_ui.main_time_label, "%02d:%02d", calendar.hour, calendar.min);
        }
	}
	
    /* 更新时间 */
    // lv_label_set_text_fmt(lv_app_ui.app_small_ui.small_cont_timer,"%02d:%02d:%02d",calendar.hour, calendar.min, calendar.sec);
    // if (lv_app_ui.lock_screen.lock_screen_cont != NULL)
    // {
    //     /* 防止锁屏界面被删除 */
    //     lv_obj_update_layout(lv_app_ui.main_cont);
    //     /* 再一次判断 */
    //     if (lv_app_ui.lock_screen.lock_screen_cont != NULL)
    //     {
    //         lv_label_set_text_fmt(lv_app_ui.lock_screen.lock_screen_date,"%02d:%02d:%02d", calendar.hour, calendar.min, calendar.sec);
    //         lv_label_set_text_fmt(lv_app_ui.lock_screen.lock_screen_time,"%04d-%02d-%02d",calendar.year, calendar.month, calendar.date);
    //         lv_label_set_text_fmt(lv_app_ui.lock_screen.lock_screen_week,"%s",weekdays[calendar.week]);
    //     }

    // }

	key = key_scan(0);
	
	if(key == BOOT_PRES)   
	{
		LED1_TOGGLE();
	}
	else if(key == KEY0_PRES)   
	{
		LED0_TOGGLE();
	}

	if(lv_general.usb_jtag_check_en == 1)
	{
		lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_usb,lv_color_make(0,255,0), LV_STATE_DEFAULT);
	}
	else if(lv_general.usb_jtag_check_en == 0)
	{
		lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_usb,lv_color_make(255,255,255), LV_STATE_DEFAULT);
	}

    if (lv_general.sd_check_en == SD_CONNET)
    {
        lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_sd,lv_color_make(0,255,0), LV_STATE_DEFAULT);

        /* 轮询SD卡设备 */
        // if (sdmmc_find() != ESP_OK)
        // {
        //     lv_general.sd_check_en = SD_DISCONNECT;
        // }

        if (lv_general.del_parent != NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(300));
            lv_msgbox_close(lv_general.del_parent);
            lv_general.del_parent = NULL;
        }
    }
    else if (lv_general.sd_check_en == SD_DISCONNECT)
    {
        lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_sd,lv_color_make(255,255,255), LV_STATE_DEFAULT);

        // if (sdmmc_init() == ESP_OK)
        // {
        //     /* 成功初始化之后，再一次判定SD卡是否插入 */
        //     if (sdmmc_find() == ESP_OK)
        //     {
        //         lv_general.sd_check_en = SD_CONNET;
        //     }
        // }

        if (lv_video_ui.video_main_ui != NULL || lv_pic_ui.pic_main_ui != NULL || lv_music_ui.music_main_ui != NULL)
        {
            if (lv_general.current_parent != NULL)
            {
                lv_obj_clear_flag(lv_app_ui.main_cont,LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(lv_general.current_parent,LV_OBJ_FLAG_HIDDEN);
                xSemaphoreGive(lv_xGuiSemaphore_handle);
            }
        }
    }
}

