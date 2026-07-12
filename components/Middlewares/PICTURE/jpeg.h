/**
 ******************************************************************************
 * @file        jpeg.h
 * @version     V1.0
 * @brief       图片解码-jpeg解码 代码
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __JPEG_H_
#define __JPEG_H_

#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "piclib.h"
#include "esp_vfs_fat.h"
#include "driver/jpeg_decode.h"

typedef void (*lcd_jpg_write_cb)(uint16_t w,uint16_t h,uint8_t *video_buf);
/* 函数声明 */
TickType_t jpeg_decode(const char *filename, int width, int height,lcd_jpg_write_cb lcd_cb); /* JPEG解码 */

#endif
