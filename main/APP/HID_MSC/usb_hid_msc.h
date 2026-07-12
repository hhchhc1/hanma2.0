/**
 ******************************************************************************
 * @file        usb_hid_msc.h
 * @version     V1.0
 * @brief       USB读取U盘驱动
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __USB_HID_MSC_H
#define __USB_HID_MSC_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"


/* USB消息定义 */
typedef struct {
    enum {
        USB_QUIT,                /* USB请求退出事件 */
        USB_DEVICE_CONNECTED,    /* USB连接事件 */
        USB_DEVICE_DISCONNECTED, /* USB断开事件 */
    } id;
    union {
        uint8_t new_dev_address; /* 新的设备地址 */
    } data;
} usb_message_t;

/* 外部调用 */
extern  QueueHandle_t usb_queue;

/* 函数声明 */
esp_err_t usb_hid_msc_init(void);

#endif
