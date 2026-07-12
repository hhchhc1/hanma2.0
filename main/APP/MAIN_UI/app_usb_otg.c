/**
 ******************************************************************************
 * @file        app_usb_otg.c
 * @version     V1.0
 * @brief       USB_OTG APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_usb_otg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"

/* USB OTG UI实例 */
usb_otg_ui_t lv_usb_otg_ui;

/* USB任务配置 */
#define USB_TASK_PRIO      5
#define USB_TASK_STK_SIZE  4 * 1024
TaskHandle_t usb_task_handler = NULL;
static const char *USB_TAG = "USB_OTG_APP";

/* 挂载路径 */
#define USB_MNT_PATH "/usb:"

/* 声明图片 */
LV_IMG_DECLARE(usb_otg);

/**
 * @brief       更新USB显示信息
 * @param       connected: 连接状态 (1:已连接, 0:未连接)
 * @param       capacity: 容量 (MB)
 * @retval      None
 */
void update_usb_display(uint8_t connected, uint64_t capacity)
{
    if (lv_usb_otg_ui.status_label == NULL) return;
    
    if (lvgl_port_lock(0))
    {
        if (connected)
        {
            lv_label_set_text(lv_usb_otg_ui.status_label, "USB Status: Connected");
            lv_obj_set_style_text_color(lv_usb_otg_ui.status_label, 
                                        lv_color_hex(0x00FF00), LV_STATE_DEFAULT);
            
            char capacity_text[32];
            snprintf(capacity_text, sizeof(capacity_text), "Capacity: %llu MB", capacity);
            lv_label_set_text(lv_usb_otg_ui.capacity_label, capacity_text);
			lv_label_set_text(lv_usb_otg_ui.device_info_label, "Device: USB Inserted");
			lv_obj_set_style_img_recolor(lv_usb_otg_ui.usb_img, lv_color_hex(0x00FF00), LV_STATE_DEFAULT);   /* 已连接 */
        }
        else
        {
            lv_label_set_text(lv_usb_otg_ui.status_label, "USB Status: Disconnected");
            lv_obj_set_style_text_color(lv_usb_otg_ui.status_label, 
                                        lv_color_hex(0xFF0000), LV_STATE_DEFAULT);
            lv_label_set_text(lv_usb_otg_ui.capacity_label, "Capacity: -- MB");
            lv_label_set_text(lv_usb_otg_ui.device_info_label, "Device: Please insert USB drive");
			lv_obj_set_style_img_recolor(lv_usb_otg_ui.usb_img, lv_color_hex(0xFF0000), LV_STATE_DEFAULT);   /* 未连接 */
        }
        
        lvgl_port_unlock();
    }
}

/**
 * @brief       USB检查任务
 * @param       pvParameters: 任务参数
 * @retval      None
 */
void usb_check_task(void *pvParameters)
{
    (void)pvParameters;
    usb_message_t msg = {0};
    
    ESP_LOGI(USB_TAG, "USB check task started");
    
    while (1)
    {
        /* 等待USB消息 */
        if (xQueueReceive(usb_queue, &msg, portMAX_DELAY))
        {
            if (msg.id == USB_DEVICE_CONNECTED)
            {
                ESP_LOGI(USB_TAG, "USB drive connected");
                
                /* 安装设备 */
                esp_err_t ret = msc_host_install_device(msg.data.new_dev_address, 
                                                        &lv_usb_otg_ui.msc_device);
                if (ret == ESP_OK)
                {
                    /* 使用文件系统挂载U盘 */
                    const esp_vfs_fat_mount_config_t mount_config = {
                        .format_if_mount_failed = false,
                        .max_files = 5,
                        .allocation_unit_size = 8192,
                    };
                    
                    /* 注册文件系统 */
                    ret = msc_host_vfs_register(lv_usb_otg_ui.msc_device, 
                                                USB_MNT_PATH, 
                                                &mount_config, 
                                                &lv_usb_otg_ui.vfs_handle);
                    
                    if (ret == ESP_OK)
                    {
                        /* 获取设备信息 */
                        msc_host_device_info_t info;
                        if (msc_host_get_device_info(lv_usb_otg_ui.msc_device, &info) == ESP_OK)
                        {
                            uint64_t capacity = ((uint64_t)info.sector_size * info.sector_count) 
                                               / (1024 * 1024);
                            
                            /* 更新显示 */
                            update_usb_display(1, capacity);
                            lv_usb_otg_ui.usb_connected = 1;
                            ESP_LOGI(USB_TAG, "USB drive mounted successfully, capacity: %llu MB", capacity);
                        }
                    }
                    else
                    {
                        ESP_LOGE(USB_TAG, "USB drive filesystem mount failed");
                        update_usb_display(0, 0);
                    }
                }
                else
                {
                    ESP_LOGE(USB_TAG, "Device installation failed");
                    update_usb_display(0, 0);
                }
            }
            else if (msg.id == USB_DEVICE_DISCONNECTED)
            {
                ESP_LOGI(USB_TAG, "USB drive disconnected");
                
                if (lv_usb_otg_ui.vfs_handle)
                {
                    msc_host_vfs_unregister(lv_usb_otg_ui.vfs_handle);
                    lv_usb_otg_ui.vfs_handle = NULL;
                }
                
                if (lv_usb_otg_ui.msc_device)
                {
                    msc_host_uninstall_device(lv_usb_otg_ui.msc_device);
                    lv_usb_otg_ui.msc_device = NULL;
                }
                
                update_usb_display(0, 0);
                lv_usb_otg_ui.usb_connected = 0;
            }
        }
    }
}

/**
 * @brief       刷新按钮事件回调
 * @param       e: 事件
 * @retval      None
 */
static void refresh_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED)
    {
        if (lv_usb_otg_ui.usb_connected && lv_usb_otg_ui.msc_device)
        {
            /* 重新获取设备信息 */
            msc_host_device_info_t info;
            if (msc_host_get_device_info(lv_usb_otg_ui.msc_device, &info) == ESP_OK)
            {
                uint64_t capacity = ((uint64_t)info.sector_size * info.sector_count) 
                                   / (1024 * 1024);
                
                update_usb_display(1, capacity);
            }
        }
    }
}

/**
 * @brief       返回按钮事件回调
 * @param       e: 事件
 * @retval      None
 */
static void back_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED)
    {
        lv_app_usb_otg_del();
    }
}

/**
 * @brief       删除USB OTG应用
 * @param       None
 * @retval      None
 */
void lv_app_usb_otg_del(void)
{
    if (usb_task_handler != NULL)
    {
        vTaskDelete(usb_task_handler);
        usb_task_handler = NULL;
    }
    
    if (lv_usb_otg_ui.usb_main_ui != NULL)
    {
        lv_obj_del(lv_usb_otg_ui.usb_main_ui);
        lv_usb_otg_ui.usb_main_ui = NULL;
    }
    
    /* 恢复主界面 */
    lv_obj_clear_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
    for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
    {
        lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
    }
    
    lv_general.current_parent = NULL;
    lv_general.del_function = NULL;
    
    ESP_LOGI(USB_TAG, "USB OTG app closed");
}

/**
 * @brief       初始化USB OTG应用
 * @param       None
 * @retval      None
 */
void lv_app_usb_otg_init(void)
{
    static uint8_t usb_init_flag = 0;
    
    /* 隐藏主界面 */
    lv_obj_add_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
    for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
    {
        lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
    }

	// /* 如果UI已经存在，先清理 */
    // if (lv_usb_otg_ui.usb_main_ui != NULL)
    // {
    //     lv_obj_del(lv_usb_otg_ui.usb_main_ui);
    //     lv_usb_otg_ui.usb_main_ui = NULL;
    // }
    
    /* 重置UI相关指针 */
    lv_usb_otg_ui.status_label = NULL;
    lv_usb_otg_ui.capacity_label = NULL;
    lv_usb_otg_ui.device_info_label = NULL;
    lv_usb_otg_ui.refresh_btn = NULL;
    lv_usb_otg_ui.back_btn = NULL;
    
    /* 创建USB主界面 */
    lv_usb_otg_ui.usb_main_ui = lv_obj_create(lv_scr_act());
    lv_obj_set_size(lv_usb_otg_ui.usb_main_ui, lv_obj_get_width(lv_scr_act()), 
                    lv_obj_get_height(lv_scr_act()) - lv_obj_get_height(lv_scr_act()) / 20);
    lv_obj_set_pos(lv_usb_otg_ui.usb_main_ui, 0, lv_obj_get_height(lv_scr_act()) / 20);
    lv_obj_set_style_bg_color(lv_usb_otg_ui.usb_main_ui, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(lv_usb_otg_ui.usb_main_ui, 0, LV_STATE_DEFAULT);
	lv_obj_set_style_radius(lv_usb_otg_ui.usb_main_ui, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(lv_usb_otg_ui.usb_main_ui, LV_OBJ_FLAG_SCROLLABLE);
    
    /* 标题 */
    lv_obj_t *title_label = lv_label_create(lv_usb_otg_ui.usb_main_ui);
    lv_label_set_text(title_label, "USB OTG Detection");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_48, LV_STATE_DEFAULT);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);
    
    /* 状态标签 */
    lv_usb_otg_ui.status_label = lv_label_create(lv_usb_otg_ui.usb_main_ui);
    lv_label_set_text(lv_usb_otg_ui.status_label, "USB Status: Detecting...");
    lv_obj_set_style_text_color(lv_usb_otg_ui.status_label, lv_color_hex(0xFFFF00), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lv_usb_otg_ui.status_label, &lv_font_montserrat_40, LV_STATE_DEFAULT);
    lv_obj_align(lv_usb_otg_ui.status_label, LV_ALIGN_TOP_MID, 0, 80);
    
    /* 容量标签 */
    lv_usb_otg_ui.capacity_label = lv_label_create(lv_usb_otg_ui.usb_main_ui);
    lv_label_set_text(lv_usb_otg_ui.capacity_label, "Capacity: -- MB");
    lv_obj_set_style_text_color(lv_usb_otg_ui.capacity_label, lv_color_hex(0x00AAFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lv_usb_otg_ui.capacity_label, &lv_font_montserrat_40, LV_STATE_DEFAULT);
    lv_obj_align(lv_usb_otg_ui.capacity_label, LV_ALIGN_TOP_MID, 0, 120);
    
    /* 设备信息标签 */
    lv_usb_otg_ui.device_info_label = lv_label_create(lv_usb_otg_ui.usb_main_ui);
    lv_label_set_text(lv_usb_otg_ui.device_info_label, "Device: --");
    lv_obj_set_style_text_color(lv_usb_otg_ui.device_info_label, lv_color_hex(0xAAAAAA), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lv_usb_otg_ui.device_info_label, &lv_font_montserrat_36, LV_STATE_DEFAULT);
    lv_obj_align(lv_usb_otg_ui.device_info_label, LV_ALIGN_TOP_MID, 0, 160);

	/* 创建USB图标 */
    lv_usb_otg_ui.usb_img = lv_img_create(lv_usb_otg_ui.usb_main_ui);
    lv_img_set_src(lv_usb_otg_ui.usb_img, &usb_otg);
    lv_obj_align(lv_usb_otg_ui.usb_img, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_img_recolor(lv_usb_otg_ui.usb_img, lv_color_hex(0x808080), LV_STATE_DEFAULT);      /* 初始化样式：默认灰色 */
    lv_obj_set_style_img_recolor_opa(lv_usb_otg_ui.usb_img, LV_OPA_COVER, LV_STATE_DEFAULT);
    
    // /* 刷新按钮 */
    // lv_usb_otg_ui.refresh_btn = lv_btn_create(lv_usb_otg_ui.usb_main_ui);
    // lv_obj_set_size(lv_usb_otg_ui.refresh_btn, 120, 50);
	// lv_obj_set_style_bg_color(lv_usb_otg_ui.refresh_btn, lv_color_hex(0x808080),LV_STATE_DEFAULT);
    // lv_obj_align(lv_usb_otg_ui.refresh_btn, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    // lv_obj_add_event_cb(lv_usb_otg_ui.refresh_btn, refresh_btn_event_cb, LV_EVENT_ALL, NULL);
    
    // lv_obj_t *refresh_label = lv_label_create(lv_usb_otg_ui.refresh_btn);
    // lv_label_set_text(refresh_label, "Refresh");
    // lv_obj_center(refresh_label);
    
    // /* 返回按钮 */
    // lv_usb_otg_ui.back_btn = lv_btn_create(lv_usb_otg_ui.usb_main_ui);
    // lv_obj_set_size(lv_usb_otg_ui.back_btn, 120, 50);
	// lv_obj_set_style_bg_color(lv_usb_otg_ui.back_btn, lv_color_hex(0x808080),LV_STATE_DEFAULT);
    // lv_obj_align(lv_usb_otg_ui.back_btn, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
    // lv_obj_add_event_cb(lv_usb_otg_ui.back_btn, back_btn_event_cb, LV_EVENT_ALL, NULL);
    
    // lv_obj_t *back_label = lv_label_create(lv_usb_otg_ui.back_btn);
    // lv_label_set_text(back_label, "Back");
    // lv_obj_center(back_label);
    
    /* 初始化USB OTG */
    if (usb_init_flag == 0)
    {
        usb_hid_msc_init();
        usb_init_flag = 1;
    }
    
    /* 创建USB检查任务 */
    if (usb_task_handler == NULL)
    {
        xTaskCreatePinnedToCore((TaskFunction_t)usb_check_task,
                                (const char*)"usb_check",
                                (uint16_t)USB_TASK_STK_SIZE,
                                (void*)NULL,
                                (UBaseType_t)USB_TASK_PRIO,
                                (TaskHandle_t*)&usb_task_handler,
                                (BaseType_t)1);
    }
    
    /* 设置当前父对象和删除函数 */
    lv_general.current_parent = lv_usb_otg_ui.usb_main_ui;
    // lv_general.del_function = lv_app_usb_otg_del;

	/* 检查当前USB连接状态并更新显示 */
    if (lv_usb_otg_ui.usb_connected && lv_usb_otg_ui.msc_device)
    {
        /* 获取设备信息 */
        msc_host_device_info_t info;
        if (msc_host_get_device_info(lv_usb_otg_ui.msc_device, &info) == ESP_OK)
        {
            uint64_t capacity = ((uint64_t)info.sector_size * info.sector_count) 
                               / (1024 * 1024);
            
            /* 更新显示 */
            update_usb_display(1, capacity);
            ESP_LOGI(USB_TAG, "USB drive already connected, capacity: %llu MB", capacity);
        }
        else
        {
            update_usb_display(0, 0);
            lv_usb_otg_ui.usb_connected = 0;
        }
    }
    else
    {
        /* 没有USB设备连接 */
        update_usb_display(0, 0);
    }
    
    ESP_LOGI(USB_TAG, "USB OTG app started");
}