/**
 ******************************************************************************
 * @file        usb_hid_msc.c
 * @version     V1.0
 * @brief       USB读取U盘驱动
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "usb_hid_msc.h"

/* 消息队列 */
QueueHandle_t usb_queue = NULL;


/**
 * @brief       MSC设备回调（连接/断开）
 * @param       event:MSC事件
 * @param       arg:传入参数
 * @retval      无
 */
static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    /* 连接？ */
    if (event->event == MSC_DEVICE_CONNECTED)
    {
        /* 发现usb host 已插入U盘 */
        usb_message_t message = {
            .id = USB_DEVICE_CONNECTED,
            .data.new_dev_address = event->device.address,
        };
        xQueueSend(usb_queue, &message, portMAX_DELAY);
    }/* 断开？ */
    else if (event->event == MSC_DEVICE_DISCONNECTED)
    {
        /* 发现usb host 未检测到U盘 */
        usb_message_t message = {
            .id = USB_DEVICE_DISCONNECTED,
        };
        xQueueSend(usb_queue, &message, portMAX_DELAY);
    }
}

/**
 * @brief       USB轮询任务
 * @param       args:未使用
 * @retval      无
 */
static void usb_task_fun(void *args)
{
    /* usb host配置*/
    const usb_host_config_t host_config = { .intr_flags = ESP_INTR_FLAG_LEVEL1 };
    /* usb host初始化 */
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    /* msc host设备配置 */
    const msc_host_driver_config_t msc_config = {
        .create_backround_task = true,  /* 创建回调任务 */
        .task_priority = 5,             /* 任务优先级 */
        .stack_size = 4096,             /* 任务堆栈大小 */
        .callback = msc_event_cb,       /* msc事件回调函数 */
    };
    /* msc host安装 */
    ESP_ERROR_CHECK(msc_host_install(&msc_config));

    bool has_clients = true;

    while (1)
    {
        uint32_t event_flags;
        /* 处理USB事件处理器 */
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        /* 所有的客户端已从主机注销了吗？ */
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            has_clients = false;
            /* 释放usb host内存 */
            if (usb_host_device_free_all() == ESP_OK)
            {
                break;
            };
        }
        /* 主机已释放所有设备？ */
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE && !has_clients)
        {
            break;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    /* 注销usb host */
    ESP_ERROR_CHECK(usb_host_uninstall());
    /* 删除usb轮询任务 */
    vTaskDelete(NULL);
}

/**
 * @brief       USB读取U盘初始化
 * @param       无
 * @retval      ESP_OK:初始化成功
 */
esp_err_t usb_hid_msc_init(void)
{
    /* 创建新的消息队列 */
    usb_queue = xQueueCreate(5, sizeof(usb_message_t));
    assert(usb_queue);

    BaseType_t usb_task = xTaskCreate(usb_task_fun, "usb_task", 4096, NULL, 2, NULL);
    assert(usb_task);

    return ESP_OK;
}
