/**
 ******************************************************************************
 * @file        app_camera.c
 * @version     V1.0
 * @brief       Camera APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_camera.h"

camera_ui_t lv_camera_ui;

/**
  * @brief  相机应用退出函数
  * @param  无
  * @retval 无
  */
void app_camera_exit(void)
{
    ESP_LOGI("CAMERA", "Camera app exiting...");  
    
    /* 停止摄像头任务 */
    mipi_cam_stop();
    
    /* 等待摄像头完全停止，确保DMA传输结束 */
    vTaskDelay(pdMS_TO_TICKS(100));  
    
    /* 删除相机界面 */
    if(lv_camera_ui.camera_main_ui != NULL) {
        lv_obj_del(lv_camera_ui.camera_main_ui);
        lv_camera_ui.camera_main_ui = NULL;
    }
    
    /* 强制刷新显示缓冲区，清除可能的残留数据 */
    lv_obj_invalidate(lv_scr_act()); 
    
    /* 显示主界面 */
    lv_obj_clear_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
    
    /* 强制重绘主界面，避免花屏 */
    lv_obj_invalidate(lv_app_ui.main_cont);  
    lv_refr_now(NULL);  
    
    /* 重新启用主界面按钮点击 */
    for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
    {
        lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
    }
    
    /* 清除当前父对象指针 */
    lv_general.current_parent = NULL;
    
    /* 再次强制刷新，确保界面正常 */
    vTaskDelay(pdMS_TO_TICKS(50));  
    lv_obj_invalidate(lv_scr_act());  
    
    ESP_LOGI("CAMERA", "Camera app exited successfully");  
}

void lv_camera_back_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_CLICKED) {
        app_camera_exit();
    }
}

void app_camera_ui_init(void)
{
	ESP_LOGI("CAMERA", "Initializing camera UI...");

    /* 如果相机界面已经存在，先清理 */
    if (lv_camera_ui.camera_main_ui != NULL) {
        app_camera_exit();
		vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    /* 创建相机主界面容器 */
    lv_camera_ui.camera_main_ui = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(lv_camera_ui.camera_main_ui, lv_color_hex(0x000000), LV_STATE_DEFAULT);                  /* 设置背景颜色 */
    lv_obj_set_size(lv_camera_ui.camera_main_ui, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()) - lv_obj_get_height(lv_scr_act()) / 20);  /* 设置容器大小 */
    lv_obj_set_style_radius(lv_camera_ui.camera_main_ui, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
    lv_obj_set_style_border_opa(lv_camera_ui.camera_main_ui, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
    lv_obj_set_pos(lv_camera_ui.camera_main_ui, 0, lv_obj_get_height(lv_scr_act()) / 20);                              /* 设置位置 */
    lv_obj_clear_flag(lv_camera_ui.camera_main_ui, LV_OBJ_FLAG_SCROLLABLE);                                            /* 禁止滚动 */
    lv_obj_update_layout(lv_camera_ui.camera_main_ui);

    /* 创建摄像头预览区域 */
    lv_camera_ui.camera_preview = lv_obj_create(lv_camera_ui.camera_main_ui);
    lv_obj_set_size(lv_camera_ui.camera_preview, lv_obj_get_width(lv_camera_ui.camera_main_ui), lv_obj_get_height(lv_camera_ui.camera_main_ui) - 100); 
    lv_obj_set_style_bg_color(lv_camera_ui.camera_preview, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(lv_camera_ui.camera_preview, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_align(lv_camera_ui.camera_preview, LV_ALIGN_TOP_MID, 0, 0);

    /* 创建底部控制栏 */
    lv_camera_ui.control_bar = lv_obj_create(lv_camera_ui.camera_main_ui);
    lv_obj_set_size(lv_camera_ui.control_bar, lv_obj_get_width(lv_camera_ui.camera_main_ui), 80);
    lv_obj_set_style_bg_color(lv_camera_ui.control_bar, lv_color_hex(0x1a1a1a), LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(lv_camera_ui.control_bar, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_align(lv_camera_ui.control_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    /* 添加返回按钮 */
    lv_camera_ui.back_btn = lv_btn_create(lv_camera_ui.control_bar);
    lv_obj_set_size(lv_camera_ui.back_btn, 60, 60);
    lv_obj_align(lv_camera_ui.back_btn, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_set_style_bg_color(lv_camera_ui.back_btn, lv_color_hex(0x444444), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(lv_camera_ui.back_btn, 30, LV_STATE_DEFAULT);
    
    lv_camera_ui.back_label = lv_label_create(lv_camera_ui.back_btn);
    lv_label_set_text(lv_camera_ui.back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lv_camera_ui.back_label, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lv_camera_ui.back_label, lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    lv_obj_center(lv_camera_ui.back_label);
    
    /* 为返回按钮添加事件回调 */
    lv_obj_add_event_cb(lv_camera_ui.back_btn, lv_camera_back_event_handler, LV_EVENT_CLICKED, NULL);

    /* 设置当前父对象，用于返回主界面 */
    lv_general.current_parent = lv_camera_ui.camera_main_ui;

	/* 刷新UI，确保界面创建完成 */
    lv_refr_now(NULL);  
    vTaskDelay(pdMS_TO_TICKS(50));  
    
    /* 初始化摄像头硬件 */
    ESP_LOGI("CAMERA", "Starting camera hardware initialization...");  
    
    /* 初始化摄像头硬件 */
    esp_err_t ret = mipi_cam_init();
    if (ret != ESP_OK) {
        ESP_LOGE("CAMERA", "Camera initialization failed");
		// lv_camera_ui.preview_label = lv_label_create(lv_camera_ui.camera_preview);
		// lv_obj_set_style_text_font(lv_camera_ui.preview_label, &lv_font_montserrat_24, LV_STATE_DEFAULT);
		// lv_obj_set_style_text_color(lv_camera_ui.preview_label, lv_color_hex(0xff0000), LV_STATE_DEFAULT);
		// lv_obj_align(lv_camera_ui.preview_label, LV_ALIGN_CENTER, 0, 0);
        // lv_label_set_text(lv_camera_ui.preview_label, "Camera Init Failed");
		vTaskDelay(pdMS_TO_TICKS(100));
		app_camera_exit();
		lv_msgbox("Camera Init Failed");
    } else {
        ESP_LOGI("CAMERA", "Camera initialization success");
        
        /* 等待摄像头稳定 */
        vTaskDelay(pdMS_TO_TICKS(200));  
        
        /* 刷新显示，避免初始花屏 */
        lv_obj_invalidate(lv_camera_ui.camera_preview);  
        lv_refr_now(NULL);  
    }
}