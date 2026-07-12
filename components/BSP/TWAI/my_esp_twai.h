/**
 ******************************************************************************
 * @file        my_esp_twai.h
 * @version     V1.0
 * @brief       TWAI驱动代码
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __MY_ESP_TWAI_H
#define __MY_ESP_TWAI_H

#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_err.h"
#include "string.h"
#include "esp_log.h"

/* TWAI通信引脚定义 */
#define TWAI_TX_GPIO_PIN        GPIO_NUM_33
#define TWAI_RX_GPIO_PIN        GPIO_NUM_34

#define MSG_ID                  0x12

/* 函数声明 */
esp_err_t twai_init(twai_mode_t mode);                              /* TWAI初始化 */
esp_err_t twai_send_data(uint32_t id, uint8_t msg[], uint8_t len);  /* TWAI发送一帧数据 */
esp_err_t twai_receive_data(uint32_t id, uint8_t buf[]);            /* TWAI接收数据查询 */

#endif
