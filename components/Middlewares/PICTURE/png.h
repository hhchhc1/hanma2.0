/**
 ******************************************************************************
 * @file        png.h
 * @version     V1.0
 * @brief       图片解码库 代码
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __PNG_H_
#define __PNG_H_

#include <stdint.h>
#include <stdbool.h>
#include "pngle.h"
#include "piclib.h"
#include "ff.h"
#include "esp_log.h"

typedef void (*lcd_png_write_cb)(uint16_t w,uint16_t h,uint8_t *video_buf);
/* 函数声明 */
void png_init(pngle_t *pngle, uint32_t w, uint32_t h);                                              /* PNG库初始化 */
void png_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]);     /* PNG绘画 */
void png_finish(pngle_t *pngle);                                                                    /* PNG解码完成回调函数 */
TickType_t png_decode(const char *filename, int width, int height,lcd_png_write_cb lcd_cb);         /* PNG图片解码 */

#endif
