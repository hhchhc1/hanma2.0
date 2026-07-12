/**
 ******************************************************************************
 * @file        app_music.h
 * @version     V1.0
 * @brief       音乐 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef __APP_MUSIC_H__
#define __APP_MUSIC_H__

#include "lvgl.h"
#include <stdio.h>
#include <math.h>
#include "app_main_ui.h"
#include "app_pic.h"

#define WAV_TX_BUFSIZE    8192

typedef struct
{
    uint32_t ChunkID;
    uint32_t ChunkSize;
    uint32_t Format;
}ChunkRIFF;     /* RIFF块 */

typedef struct
{
    uint32_t ChunkID;
    uint32_t ChunkSize;
    uint16_t AudioFormat;
    uint16_t NumOfChannels;
    uint32_t SampleRate;
    uint32_t ByteRate;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;
//    uint16_t ByteExtraData;
}ChunkFMT;      /* fmt */

typedef struct 
{
    uint32_t ChunkID;
    uint32_t ChunkSize;
    uint32_t NumOfSamples;
}ChunkFACT;     /* fact */

typedef struct 
{
    uint32_t ChunkID;
    uint32_t ChunkSize;
}ChunkLIST;     /* LIST */

typedef struct
{
    uint32_t ChunkID;
    uint32_t ChunkSize;
}ChunkDATA;     /* data */

typedef struct
{ 
    ChunkRIFF riff;
    ChunkFMT fmt;
//    ChunkFACT fact;
    ChunkDATA data;
}__WaveHeader;  /* wav */

typedef struct
{ 
    uint16_t audioformat;
    uint16_t nchannels;
    uint16_t blockalign;
    uint32_t datasize;

    uint32_t totsec;
    uint32_t cursec;

    uint32_t bitrate;
    uint32_t samplerate;
    uint16_t bps;

    uint32_t datastart;
}__wavctrl;

typedef struct
{
    uint8_t *tbuf;
    FIL *file;

    uint8_t status;
}__audiodev;

enum MUSIC_STATE
{
    MUSIC_NULL,
    MUSIC_PAUSE,
    MUSIC_PLAY,
    MUSIC_NEXT,
    MUSIC_PREV
};

/* 结构体声明 */
typedef struct {
    lv_obj_t * music_main_ui;           /* 音乐 UI容器 */
    lv_obj_t * music_img;               /* 音乐 APP img */
    lv_obj_t * music_bar;               /* 音乐 bar */
    lv_obj_t * music_label_left_data;   /* 音乐左侧时间 */
    lv_obj_t * music_label_right_data;  /* 音乐右侧时间 */
    lv_obj_t * music_next;              /* 下一首音乐 */
    lv_obj_t * music_prev;              /* 上一首音乐右侧时间 */
    lv_obj_t * music_start_pause;       /* 开始暂停音乐 */
    lv_obj_t *music_arc_center;        /* 音乐进度条 */
    lv_obj_t *song_play_label;
    lv_obj_t *song_last_label;
    lv_obj_t *song_next_label;

    FF_DIR wavdir;                      /* wav文件目录 */
    uint16_t totwavnum;                 /* wav文件总数 */
    FILINFO *wavfileinfo;
    uint8_t *pname;
    uint32_t *wavoffsettbl;
    uint16_t curindex;
} music_ui_t;

extern music_ui_t lv_music_ui;

/* 函数声明 */
void lv_app_music_init(void);

#endif