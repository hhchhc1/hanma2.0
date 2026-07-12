/**
 ******************************************************************************
 * @file        app_usb_otg.h
 * @version     V1.0
 * @brief       USB_OTG APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __APP_USB_OTG_H__
#define __APP_USB_OTG_H__

#include "lvgl.h"
#include <stdio.h>
#include "lcd.h"
#include "app_main_ui.h"
#include "usb_hid_msc.h"
#include "ff.h"
#include "sdmmc.h"

/* USB OTG UI结构体 */
typedef struct {
    lv_obj_t *usb_main_ui;         /* USB主UI容器 */
    lv_obj_t *status_label;        /* 状态标签 */
    lv_obj_t *capacity_label;      /* 容量标签 */
    lv_obj_t *device_info_label;   /* 设备信息标签 */
    lv_obj_t *refresh_btn;         /* 刷新按钮 */
    lv_obj_t *back_btn;            /* 返回按钮 */
	lv_obj_t* usb_img;             /* USB图标 */
    msc_host_device_handle_t msc_device;  /* MSC设备句柄 */
    msc_host_vfs_handle_t vfs_handle;     /* VFS文件系统句柄 */
    uint8_t usb_connected;                /* USB连接状态 */
} usb_otg_ui_t;

/* 外部变量声明 */
extern usb_otg_ui_t lv_usb_otg_ui;

/* 函数声明 */
void lv_app_usb_otg_init(void);
void lv_app_usb_otg_del(void);
void usb_check_task(void *pvParameters);
void update_usb_display(uint8_t connected, uint64_t capacity);

#endif /* __APP_USB_OTG_H__ */