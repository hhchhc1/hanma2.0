/**
 ******************************************************************************
 * @file        app_video_ui.c
 * @version     V1.0
 * @brief       视频 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_video_ui.h"


video_ui_t lv_video_ui;

QueueSetHandle_t        video_xQueueSet;                 /* 定义队列集 */
QueueHandle_t           video_xSemaphore_exit;           /* 定义下一个video开关信号量 */
SemaphoreHandle_t       video_xSemaphore_next;           /* 定义上一个video开关信号量 */
SemaphoreHandle_t       video_xSemaphore_prev;           /* 定义上一个video开关信号量 */
SemaphoreHandle_t       video_xSemaphore_starst_pause;  /* 暂停和开启 */

/* VIDEO Task Configuration
 * Including: task handle, task priority, stack size, creation task
 */
#define VIDEO_PRIO      20                              /* task priority */
#define VIDEO_STK_SIZE  5 * 1024                        /* task stack size */
TaskHandle_t            VIDEOTask_Handler;              /* task handle */
void lv_video(void *pvParameters);                      /* Task function */

extern uint16_t g_frame;                /* 播放帧率 */
extern volatile uint8_t g_frameup;      /* 视频播放时隙控制变量,当等于1的时候,可以更新下一帧视频 */

/**
 * @brief       获取指定路径下有效视频文件的数量
 * @param       path: 指定路径
 * @retval      有效视频文件的数量
 */
static uint16_t video_get_tnum(char *path)
{
    uint8_t res;
    uint16_t rval = 0;
    FF_DIR tdir;            /* 临时目录 */
    FILINFO *tfileinfo;     /* 临时文件信息 */
    
    tfileinfo = (FILINFO *)malloc(sizeof(FILINFO));             /* 申请内存 */
    res = (uint8_t)f_opendir(&tdir, (const TCHAR *)path);       /* 打开目录 */
    
    if ((res == 0) && tfileinfo)
    {
        while (1)                                               /* 查询总的有效文件数 */
        {
            res = (uint8_t)f_readdir(&tdir, tfileinfo);         /* 读取目录下的一个文件 */
            
            if ((res != 0) || (tfileinfo->fname[0] == 0))       /* 错误或到末尾，退出 */
            {
                break;
            }
            
            res = exfuns_file_type(tfileinfo->fname);
            if ((res & 0xF0) == 0x60)                           /* 是视频文件 */
            {
                rval++;
            }
        }
    }
    
    free(tfileinfo);        /* 释放内存 */
    
    return rval;
}

AVI_INFO g_avix;                                      /* avi文件相关信息 */
char *const AVI_VIDS_FLAG_TBL[2] = {"00dc", "01dc"};  /* 视频编码标志字符串,00dc/01dc */
char *const AVI_AUDS_FLAG_TBL[2] = {"00wb", "01wb"};  /* 音频编码标志字符串,00wb/01wb */

/**
 * @brief       avi解码初始化
 * @param       buf  : 输入缓冲区
 * @param       size : 缓冲区大小
 * @retval      res
 *    @arg      OK,avi文件解析成功
 *    @arg      其他,错误代码
 */
AVISTATUS avi_init(uint8_t *buf, uint32_t size)
{
    uint16_t offset;
    uint8_t *tbuf;
    AVISTATUS res = AVI_OK;
    AVI_HEADER *aviheader;
    LIST_HEADER *listheader;
    AVIH_HEADER *avihheader;
    STRH_HEADER *strhheader;

    STRF_BMPHEADER *bmpheader;
    STRF_WAVHEADER *wavheader;

    tbuf = buf;
    aviheader = (AVI_HEADER *)buf;
    if (aviheader->RiffID != AVI_RIFF_ID)
    {
        return AVI_RIFF_ERR;        /* RIFF ID错误 */
    }

    if (aviheader->AviID != AVI_AVI_ID)
    {
        return AVI_AVI_ERR;         /* AVI ID错误 */
    }

    buf += sizeof(AVI_HEADER);      /* 偏移 */
    listheader = (LIST_HEADER *)(buf);
    if (listheader->ListID != AVI_LIST_ID)
    {
        return AVI_LIST_ERR;        /* LIST ID错误 */
    }

    if (listheader->ListType != AVI_HDRL_ID)
    {
        return AVI_HDRL_ERR;        /* HDRL ID错误 */
    }

    buf += sizeof(LIST_HEADER);     /* 偏移 */
    avihheader = (AVIH_HEADER *)(buf);
    if (avihheader->BlockID != AVI_AVIH_ID)
    {
        return AVI_AVIH_ERR;        /* AVIH ID错误 */
    }

    g_avix.SecPerFrame = avihheader->SecPerFrame;   /* 得到帧间隔时间 */
    g_avix.TotalFrame = avihheader->TotalFrame;     /* 得到总帧数 */
    buf += avihheader->BlockSize + 8;               /* 偏移 */
    listheader = (LIST_HEADER *)(buf);
    if (listheader->ListID != AVI_LIST_ID)
    {
        return AVI_LIST_ERR;        /* LIST ID错误 */
    }

    if (listheader->ListType != AVI_STRL_ID)
    {
        return AVI_STRL_ERR;        /* STRL ID错误 */
    }

    strhheader = (STRH_HEADER *)(buf + 12);
    if (strhheader->BlockID != AVI_STRH_ID)
    {
        return AVI_STRH_ERR;        /* STRH ID错误 */
    }

    if (strhheader->StreamType == AVI_VIDS_STREAM)  /* 视频帧在前 */
    {
        if (strhheader->Handler != AVI_FORMAT_MJPG)
        {
            return AVI_FORMAT_ERR;  /* 非MJPG视频流,不支持 */
        }

        g_avix.VideoFLAG = AVI_VIDS_FLAG_TBL[0];    /* 视频流标记  "00dc" */
        g_avix.AudioFLAG = AVI_AUDS_FLAG_TBL[1];    /* 音频流标记  "01wb" */
        bmpheader = (STRF_BMPHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
        if (bmpheader->BlockID != AVI_STRF_ID)
        {
            return AVI_STRF_ERR;    /* STRF ID错误 */
        }

        g_avix.Width = bmpheader->bmiHeader.Width;
        g_avix.Height = bmpheader->bmiHeader.Height;
        buf += listheader->BlockSize + 8;       /* 偏移 */
        listheader = (LIST_HEADER *)(buf);
        if (listheader->ListID != AVI_LIST_ID)  /* 是不含有音频帧的视频文件 */
        {
            g_avix.SampleRate = 0;              /* 音频采样率 */
            g_avix.Channels = 0;                /* 音频通道数 */
            g_avix.AudioType = 0;               /* 音频格式 */

        }
        else
        {
            if (listheader->ListType != AVI_STRL_ID)
            {
                return AVI_STRL_ERR;    /* STRL ID错误 */
            }

            strhheader = (STRH_HEADER *)(buf + 12);
            if (strhheader->BlockID != AVI_STRH_ID)
            {
                return AVI_STRH_ERR;    /* STRH ID错误 */
            }

            if (strhheader->StreamType != AVI_AUDS_STREAM)
            {
                return AVI_FORMAT_ERR;  /* 格式错误 */
            }

            wavheader = (STRF_WAVHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
            if (wavheader->BlockID != AVI_STRF_ID)
            {
                return AVI_STRF_ERR;    /* STRF ID错误 */
            }

            g_avix.SampleRate = wavheader->SampleRate;      /* 音频采样率 */
            g_avix.Channels = wavheader->Channels;          /* 音频通道数 */
            g_avix.AudioType = wavheader->FormatTag;        /* 音频格式 */
        }
    }
    else if (strhheader->StreamType == AVI_AUDS_STREAM)     /* 音频帧在前 */
    { 
        g_avix.VideoFLAG = AVI_VIDS_FLAG_TBL[1];            /* 视频流标记  "01dc" */
        g_avix.AudioFLAG = AVI_AUDS_FLAG_TBL[0];            /* 音频流标记  "00wb" */
        wavheader = (STRF_WAVHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
        if (wavheader->BlockID != AVI_STRF_ID)
        {
            return AVI_STRF_ERR;                            /* STRF ID错误 */
        }
 
        g_avix.SampleRate = wavheader->SampleRate;          /* 音频采样率 */
        g_avix.Channels = wavheader->Channels;              /* 音频通道数 */
        g_avix.AudioType = wavheader->FormatTag;            /* 音频格式 */
        buf += listheader->BlockSize + 8;                   /* 偏移 */
        listheader = (LIST_HEADER *)(buf);
        if (listheader->ListID != AVI_LIST_ID)
        {
            return AVI_LIST_ERR;    /* LIST ID错误 */
        }

        if (listheader->ListType != AVI_STRL_ID)
        {
            return AVI_STRL_ERR;    /* STRL ID错误 */
        }

        strhheader = (STRH_HEADER *)(buf + 12);
        if (strhheader->BlockID != AVI_STRH_ID)
        {
            return AVI_STRH_ERR;    /* STRH ID错误 */
        }

        if (strhheader->StreamType != AVI_VIDS_STREAM)
        {
            return AVI_FORMAT_ERR;  /* 格式错误 */
        }

        bmpheader = (STRF_BMPHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
        if (bmpheader->BlockID != AVI_STRF_ID)
        {
            return AVI_STRF_ERR;    /* STRF ID错误 */
        }

        if (bmpheader->bmiHeader.Compression != AVI_FORMAT_MJPG)
        {
            return AVI_FORMAT_ERR;  /* 格式错误 */
        }

        g_avix.Width = bmpheader->bmiHeader.Width;
        g_avix.Height = bmpheader->bmiHeader.Height;
    }

    offset = avi_srarch_id(tbuf, size, "movi");     /* 查找movi ID */
    if (offset == 0)
    {
        return AVI_MOVI_ERR;        /* MOVI ID错误 */
    }

    if (g_avix.SampleRate)          /* 有音频流,才查找 */
    {
        tbuf += offset;
        offset = avi_srarch_id(tbuf, size, g_avix.AudioFLAG);   /* 查找音频流标记 */
        if (offset == 0)
        {
            return AVI_STREAM_ERR;  /* 流错误 */
        }
        tbuf += offset + 4;
        g_avix.AudioBufSize = *((uint16_t *)tbuf);              /* 得到音频流buf大小. */
    }

    ESP_LOGI("avi", "avi init ok");
    ESP_LOGI("avi", "g_avix.SecPerFrame:%ld", g_avix.SecPerFrame);
    ESP_LOGI("avi", "g_avix.TotalFrame:%ld",  g_avix.TotalFrame);
    ESP_LOGI("avi", "g_avix.Width:%ld",       g_avix.Width);
    ESP_LOGI("avi", "g_avix.Height:%ld",      g_avix.Height);
    ESP_LOGI("avi", "g_avix.AudioType:%d",    g_avix.AudioType);
    ESP_LOGI("avi", "g_avix.SampleRate:%ld",  g_avix.SampleRate);
    ESP_LOGI("avi", "g_avix.Channels:%d",     g_avix.Channels);
    ESP_LOGI("avi", "g_avix.AudioBufSize:%d", g_avix.AudioBufSize);
    ESP_LOGI("avi", "g_avix.VideoFLAG:%s",    g_avix.VideoFLAG);
    ESP_LOGI("avi", "g_avix.AudioFLAG:%s",    g_avix.AudioFLAG);

    return res;
}

/**
 * @brief       查找 ID
 * @param       buf  : 输入缓冲区
 * @param       size : 缓冲区大小
 * @param       id   : 要查找的id, 必须是4字节长度
 * @retval      执行结果
 *   @arg       0     , 没找到
 *   @arg       其他  , movi ID偏移量
 */
uint32_t avi_srarch_id(uint8_t *buf, uint32_t size, char *id)
{
    uint32_t i;
    uint32_t idsize = 0;
    size -= 4;
    for (i = 0; i < size; i++)
    {
        if ((buf[i] == id[0]) &&
            (buf[i + 1] == id[1]) &&
            (buf[i + 2] == id[2]) &&
            (buf[i + 3] == id[3]))
        {
            idsize = MAKEDWORD(buf + i + 4);    /* 得到帧大小,必须大于16字节,才返回,否则不是有效数据 */

            if (idsize > 0X10)return i;         /* 找到"id"所在的位置 */
        }
    }

    return 0;
}

/**
 * @brief       得到stream流信息
 * @param       buf  : 流开始地址(必须是01wb/00wb/01dc/00dc开头)
 * @retval      执行结果
 *   @arg       AVI_OK, AVI文件解析成功
 *   @arg       其他  , 错误代码
 */
AVISTATUS avi_get_streaminfo(uint8_t *buf)
{
    g_avix.StreamID = MAKEWORD(buf + 2);    /* 得到流类型 */
    g_avix.StreamSize = MAKEDWORD(buf + 4); /* 得到流大小 */

    // ESP_LOGI("avi", "g_avix.StreamSize:%ld", g_avix.StreamSize);

    if (g_avix.StreamSize > AVI_MAX_FRAME_SIZE)   /* 帧大小太大了,直接返回错误 */
    {
        ESP_LOGI("avi", "FRAME SIZE OVER:%ld", g_avix.StreamSize);
        g_avix.StreamSize = 0;
        return AVI_STREAM_ERR;
    }
    
    if (g_avix.StreamSize % 2)
    {
        g_avix.StreamSize++;    /* 奇数加1(g_avix.StreamSize,必须是偶数) */
    }

    if (g_avix.StreamID == AVI_VIDS_FLAG || g_avix.StreamID == AVI_AUDS_FLAG)
    {
        return AVI_OK;
    }

    return AVI_STREAM_ERR;
}


/**
 * @brief  video playback event callback
 * @param  *e ：event
 * @return None
 */
static void lv_video_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    static uint8_t pressed_flag = 0;
    static uint8_t start_pause_pressed_flag = 0;

    if(code == LV_EVENT_CLICKED)
    {
        if (target == lv_video_ui.video_box_t.video_start_pause)
        {
            if (start_pause_pressed_flag == 0)
            {
                lv_video_ui.key = VIDEO_PAUSE;
                lv_label_set_text(lv_video_ui.video_box_t.video_start_pause, LV_SYMBOL_PLAY);
            }   
            else if (start_pause_pressed_flag == 1)
            {
                lv_video_ui.key = VIDEO_PLAY;
                lv_label_set_text(lv_video_ui.video_box_t.video_start_pause, LV_SYMBOL_PAUSE);
            }

            start_pause_pressed_flag = !start_pause_pressed_flag;
            xSemaphoreGive(video_xSemaphore_starst_pause);
        }
        else if (target == lv_video_ui.video_box_t.video_next)
        {
            xSemaphoreGive(video_xSemaphore_next);
            lv_video_ui.key = VIDEO_NEXT;
        }
        else if (target == lv_video_ui.video_box_t.video_prev)
        {
            xSemaphoreGive(video_xSemaphore_prev);
            lv_video_ui.key = VIDEO_PREV;
        }
        else if (target == lv_video_ui.video_box_t.video_volume)
        {
            if (pressed_flag == 0)
            {
                lv_label_set_text(lv_video_ui.video_box_t.video_volume, LV_SYMBOL_MUTE);
				/* 设置ES8311音量 */
			    esp_codec_dev_set_out_vol(codec_handle, 0);
                lv_slider_set_value(lv_video_ui.video_box_t.video_volume_slider,0, LV_ANIM_ON);
                lv_obj_clear_flag(lv_video_ui.video_box_t.video_volume_slider, LV_OBJ_FLAG_CLICKABLE);
            }
            else if (pressed_flag == 1)
            {
                lv_label_set_text(lv_video_ui.video_box_t.video_volume, LV_SYMBOL_VOLUME_MAX);
				/* 设置ES8311音量 */
			    esp_codec_dev_set_out_vol(codec_handle, 20);
                lv_slider_set_value(lv_video_ui.video_box_t.video_volume_slider,20, LV_ANIM_ON);
                lv_obj_add_flag(lv_video_ui.video_box_t.video_volume_slider, LV_OBJ_FLAG_CLICKABLE);
            }

            pressed_flag = !pressed_flag;
        }
        else if (target == lv_video_ui.video_box_t.video_list)
        {
            /* 未实现 */
        }

    }
    else if (code == LV_EVENT_VALUE_CHANGED)
    {
        if (target == lv_video_ui.video_box_t.video_volume_slider)
        {
			/* 设置ES8311音量 */
			esp_codec_dev_set_out_vol(codec_handle, (int)lv_slider_get_value(target));
        }
    }
}

uint8_t *jpeg_rx_buf = NULL; 
uint32_t jpeg_alloc_size = 0;

jpeg_decoder_handle_t jpgd_handle;                      /* JPEG解码器句柄 */

jpeg_decode_cfg_t video_decode_cfg_rgb = {                    /* JPEG解码器配置参数 */
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,     /* 解码输出的颜色格式RGB565 */
    .rgb_order     = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,    /* 解码输出的RGB顺序 */
};

/**
 * @brief       初始化JPEG解码器
 * @param       width: 显示图像的宽度
 * @param       height: 显示图像的高度
 * @param       malloc_size: 申请到BUF大小
 * @retval      JPEG解码器申请到内存的地址
 */
uint8_t *mjpegdec_init(uint32_t width, uint32_t height, uint32_t *malloc_size)
{
    jpeg_decode_engine_cfg_t decode_eng_cfg = {     /* JPEG解码器引擎配置 */
        .intr_priority = 0,                         /* 中断优先级,0选择默认 */
        .timeout_ms    = -1,                        /* 超时时间 */
    };
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&decode_eng_cfg, &jpgd_handle));    /* 安装JPEG解码器驱动 */

    jpeg_decode_memory_alloc_cfg_t rx_mem_cfg = {   /* JPEG解码器内存申请配置 */
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,   /* 图像输出BUF */
    };

    return (uint8_t *)jpeg_alloc_decoder_mem(width * height * 2 * 4, &rx_mem_cfg, (size_t *)malloc_size);   /* 分配缓冲区 */
}

typedef void (*lcd_full_cb)(uint32_t w,uint32_t h,uint8_t *video_buf);

/**
 * @brief       解码一副JPEG图片
 * @param       inbuf: jpeg数据流数组
 * @param       inbuf_size: jpeg数据流大小
 * @param       outbuf: jpeg解码器申请的内存缓冲区
 * @param       outbuf_size: jpeg解码器申请到内存大小
 * @param       width: 显示图像的宽度
 * @param       height: 显示图像的高度  
 * @retval      0,成功; 1,错误帧/解码错误
 */
uint8_t mjpegdec_decode(uint8_t *inbuf, uint32_t inbuf_size,  uint8_t *outbuf, uint32_t outbuf_size, uint32_t width, uint32_t height,lcd_full_cb lv_full_cd)
{
    if (inbuf_size == 0)    /* 帧错误,跳过解码 */
    {
        return 1;
    }

    static uint32_t out_size = 0;

    ESP_ERROR_CHECK(jpeg_decoder_process(jpgd_handle, &video_decode_cfg_rgb, inbuf, inbuf_size, outbuf, outbuf_size, &out_size));     /* 解码JPEG图片 */

    if (out_size == 0)      /* 解码长度错误 */
    {
        return 1;
    }

    /* 把视频流绘制到img控件当中 */
    lv_full_cd(width,height,outbuf);

    return 0;
}

/**
 * @brief       卸载JPEG解码器并释放内存
 * @param       buf: JPEG解码器申请的内存
 * @retval      无
 */
void mjpegdec_free(uint8_t *buf)
{
    heap_caps_free(buf);                                    /* 释放JPEG解码器申请的内存 */
    ESP_ERROR_CHECK(jpeg_del_decoder_engine(jpgd_handle));  /* 卸载JPEG解码器引擎 */
}

/**
 * @brief       Display current playback time
 * @param       favi   : The currently playing video file
 * @param       aviinfo: AVI control structure
 * @retval      None
 */
void video_time_show(FIL *favi, AVI_INFO *aviinfo)
{
    static uint32_t oldsec;
    
    uint32_t totsec = 0;
    uint32_t cursec;
    
    totsec = (aviinfo->SecPerFrame / 1000) * aviinfo->TotalFrame;
    totsec /= 1000;
    cursec = ((double)favi->fptr / favi->obj.objsize) * totsec;
    
    if (oldsec != cursec)
    {
        oldsec = cursec;
        lv_slider_set_range(lv_video_ui.video_slider_obj,0,totsec);
        lv_slider_set_value(lv_video_ui.video_slider_obj,cursec,LV_ANIM_ON);
    }
}

lv_img_dsc_t video_img_dsc = {
    .header.always_zero = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};

/**
 * @brief       show video
 * @param       None
 * @retval      None
 */
void video_show(uint32_t w,uint32_t h,uint8_t * video_buf)
{
    video_img_dsc.header.w = w;
    video_img_dsc.header.h = h;
    video_img_dsc.data_size = w * h * 2;
    video_img_dsc.data = (const uint8_t *)video_buf;
    lv_img_set_src(lv_video_ui.video_img_obj,&video_img_dsc);
    lv_obj_invalidate(lv_video_ui.video_img_obj);
}
/**
 * @brief       video task
 * @param       pvParameters : parameters (not used)
 * @retval      None
 */
void lv_video(void *pvParameters)
{
    pvParameters = pvParameters;
    uint8_t res = 0;
    lv_video_ui.key = VIDEO_NULL;
    uint8_t *pbuf;                  /* buf指针 */
    UINT nr;
    uint16_t offset;               /* 文件偏移 */
    QueueSetMemberHandle_t video_activate_member = NULL;

	gpio_set_level(GPIO_NUM_11, 1);   /* 打开喇叭 */
    vTaskDelay(pdMS_TO_TICKS(20));

    res = (uint8_t)f_opendir(&lv_video_ui.vdir, "0:/VIDEO");    /* 打开目录 */

    if (res != FR_OK)
    {
        vTaskDelete(NULL);
    }

    while (1)    /* 打开成功 */
    {
        atk_dir_sdi(&lv_video_ui.vdir, lv_video_ui.voffsettbl[lv_video_ui.curindex]);   /* 改变当前目录索引 */

        res = f_readdir(&lv_video_ui.vdir, lv_video_ui.vfileinfo);                      /* 读取目录下的一个文件 */
        if ((res != FR_OK) || (lv_video_ui.vfileinfo->fname[0] == 0))
        {
            break;  /* 错误了/到末尾了,退出 */
        }

        strcpy((char *)lv_video_ui.pname, "0:/VIDEO/");                                 /* 复制路径(目录) */
        strcat((char *)lv_video_ui.pname, (const char *)lv_video_ui.vfileinfo->fname);  /* 将文件名接在后面 */
        printf("%s\n", lv_video_ui.pname);

        lv_video_ui.framebuf =  heap_caps_malloc(AVI_VIDEO_BUF_SIZE, MALLOC_CAP_DMA);
        ESP_ERROR_CHECK(esp_dma_malloc(AVI_AUDIO_BUF_SIZE * sizeof(lv_color_t), ESP_DMA_MALLOC_FLAG_PSRAM, (void *)&lv_video_ui.audiobuf[0], NULL));
        ESP_ERROR_CHECK(esp_dma_malloc(AVI_AUDIO_BUF_SIZE * sizeof(lv_color_t), ESP_DMA_MALLOC_FLAG_PSRAM, (void *)&lv_video_ui.audiobuf[1], NULL));

        lv_video_ui.audio_buffer = lv_video_ui.audiobuf[0];
        lv_video_ui.favi = (FIL *)malloc(sizeof(FIL));                                  /* 申请favi内存 */

        if ((lv_video_ui.framebuf == NULL) || (lv_video_ui.favi == NULL) || (lv_video_ui.audiobuf[0] == NULL) || (lv_video_ui.audiobuf[1] == NULL))     /* 只要最后这个视频buf申请失败, 前面的申请失不失败都不重要, 总之就是失败了 */
        {
            ESP_LOGE("videoplay", "memory error!");
            break;
        } 

        memset(lv_video_ui.framebuf,    0, AVI_VIDEO_BUF_SIZE);
        memset(lv_video_ui.audiobuf[0], 0, AVI_AUDIO_BUF_SIZE);
        memset(lv_video_ui.audiobuf[1], 0, AVI_AUDIO_BUF_SIZE);

        while (res == 0)
        {
            res = (uint8_t)f_open(lv_video_ui.favi, (const TCHAR *)lv_video_ui.pname, FA_READ);             /* 打开文件 */

            if (res == 0)
            {
                pbuf = lv_video_ui.framebuf;
                res = (uint8_t)f_read(lv_video_ui.favi, pbuf, AVI_VIDEO_BUF_SIZE, (UINT *)&nr);  /* 开始读取 */

                if (res != 0)
                {
                    ESP_LOGI("videoplay_tag", "fread error:%d", res);
                    break;
                }
                
                res = avi_init(pbuf, AVI_VIDEO_BUF_SIZE);   /* AVI解析 */

                if (res != 0)
                {
                    ESP_LOGI("videoplay_tag", "avi error:%d", res);
                    break;
                }

                frame_timer_init(g_avix.SecPerFrame);   /* 初始化ESP_TIMER,用于等待帧间隔 */

                offset = avi_srarch_id(pbuf, AVI_VIDEO_BUF_SIZE, "movi");   /* 寻找movi ID */
                avi_get_streaminfo(pbuf + offset + 4);                      /* 获取流信息 */
                f_lseek(lv_video_ui.favi, offset + 12);                                         /* 跳过标志ID，读地址偏移到流数据开始处 */

                jpeg_rx_buf = mjpegdec_init(g_avix.Width, g_avix.Height, &jpeg_alloc_size);     /* MJPEG初始化 */

                if (g_avix.SampleRate)              /* 有音频信息,才初始化 */
                {
                    // myi2s_init();                   /* 初始化i2s */  
                    vTaskDelay(pdMS_TO_TICKS(50));  /* 适当延时 */
                    i2s_set_samplerate_bits_sample(g_avix.SampleRate, I2S_BITS_PER_SAMPLE_16BIT);   /* 设置采样率和数据位宽 */
                    i2s_trx_start();                /* I2S TRX启动 */ 
                }

                while(1)
                {
                    if (g_avix.StreamID == AVI_VIDS_FLAG)                       /* 视频流 dc */
                    {
                        pbuf = lv_video_ui.framebuf;
                        f_read(lv_video_ui.favi, pbuf, g_avix.StreamSize + 8, (UINT *)&nr); /* 读取整帧+下一帧数据流ID信息 */
                        
                        res = mjpegdec_decode(pbuf, g_avix.StreamSize, jpeg_rx_buf, jpeg_alloc_size, g_avix.Width, g_avix.Height,video_show);  /* 解码一副JPEG图片 */
                        
                        if (res != 0)
                        {
                            ESP_LOGI("videoplay_tag", "illegal frame decode error!");
                        }

                        while (g_frameup == 0);   /* 等待时间到达(在mytimer.c的中断里面设置为1) */
                        g_frameup = 0;            /* 等待播放时间到达 */
                        g_frame ++;
                    }
                    else                                                        /* wb 音频 */
                    {
                        lv_video_ui.audio_buffer = lv_video_ui.audiobuf[0] == lv_video_ui.audio_buffer ? lv_video_ui.audiobuf[1] : lv_video_ui.audiobuf[0];     /* 使用双缓冲 */

                        video_time_show(lv_video_ui.favi, &g_avix);     /* 显示当前播放时间 */

                        f_read(lv_video_ui.favi, lv_video_ui.audio_buffer, g_avix.StreamSize + 8, (UINT *)&nr);     /* 填充framebuf */
                        pbuf = lv_video_ui.audio_buffer;
                        i2s_tx_write(lv_video_ui.audio_buffer, g_avix.StreamSize);              /* 数据发送给I2S */
                    }

                    video_activate_member = xQueueSelectFromSet(video_xQueueSet, 10);/* 等待队列集中的队列接收到消息 */

                    if (video_activate_member == video_xSemaphore_next)
                    {
                        xSemaphoreTake(video_activate_member, 20);
                        break;
                    }
                    else if (video_activate_member == video_xSemaphore_prev)
                    {
                        xSemaphoreTake(video_activate_member, 20);
                        break;
                    }
                    else if (video_activate_member == video_xSemaphore_exit)
                    {
                        xSemaphoreTake(video_activate_member, 20);
                        printf("exit video\n");
                        while(1)
                        {
                            vTaskDelay(pdMS_TO_TICKS(10));
                        }
                    }
                    else if (video_activate_member == video_xSemaphore_starst_pause)
                    {
                        xSemaphoreTake(video_activate_member, 20);

                        if (lv_video_ui.key == VIDEO_PAUSE)
                        {
                            while(1)
                            {
                                if (lv_video_ui.key == VIDEO_PLAY)
                                {
                                    break;
                                }

                                vTaskDelay(pdMS_TO_TICKS(10));
                            }
                        }
                    }

                    if (avi_get_streaminfo(pbuf + g_avix.StreamSize) != 0)      /* 读取下一帧流标志 */
                    {
                        lv_video_ui.key = VIDEO_NEXT;   /* 读取下一帧流标志失败,播放下一个视频 */
                        break;
                    }
                }

                f_close(lv_video_ui.favi);      /* 关闭文件 */
                // i2s_trx_stop();                 /* 关闭音频 */
                // i2s_deinit();                   /* I2S恢复到默认 */
                mjpegdec_free(jpeg_rx_buf);     /* 卸载硬件JPEG驱动并释放内存 */
                frame_timer_stop();             /* 停止定时器工作 */  
            }

            if (lv_video_ui.key == VIDEO_NEXT)           /* 下一个视频 */
            {
                lv_video_ui.curindex++;

                if (lv_video_ui.curindex >= lv_video_ui.totavinum)
                {
                    lv_video_ui.curindex = 0;           /* 到末尾的时候,自动从头开始 */
                }
            }
            else if (lv_video_ui.key == VIDEO_PREV)      /* 上一个视频 */
            {
                if (lv_video_ui.curindex)
                {
                    lv_video_ui.curindex--;
                }
                else
                {
                    lv_video_ui.curindex = lv_video_ui.totavinum - 1;
                }
            }

            lv_video_ui.key = VIDEO_NULL;
            break;
        }

        heap_caps_free(lv_video_ui.framebuf);
        free(lv_video_ui.audiobuf[0]);
        free(lv_video_ui.audiobuf[1]);
        free(lv_video_ui.favi);
    }

    heap_caps_free(lv_video_ui.framebuf);
    free(lv_video_ui.audiobuf[0]);
    free(lv_video_ui.audiobuf[1]);
    free(lv_video_ui.favi);
    free(lv_video_ui.vfileinfo);
    free(lv_video_ui.pname);
    free(lv_video_ui.voffsettbl);
    vTaskDelete(NULL);
}

/**
  * @brief  del music
  * @param  None
  * @retval None
  */
void lv_video_del(void)
{
    if (lv_video_ui.key == VIDEO_PAUSE)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    else 
    {
        xSemaphoreGive(video_xSemaphore_exit);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // i2s_trx_stop();                 /* 关闭音频 */
    // i2s_deinit();                   /* I2S恢复到默认 */
    mjpegdec_free(jpeg_rx_buf);     /* 卸载硬件JPEG驱动并释放内存 */
    frame_timer_stop();             /* 停止定时器工作 */  

    f_close(lv_video_ui.favi);
    
    if (VIDEOTask_Handler)
    {
        vTaskDelete(VIDEOTask_Handler);
        VIDEOTask_Handler = NULL;
    }

    if (lv_video_ui.framebuf || lv_video_ui.audiobuf[0] || lv_video_ui.audiobuf[1])
    {
        free(lv_video_ui.framebuf);
        free(lv_video_ui.audiobuf[0]);
        free(lv_video_ui.audiobuf[1]);
    }

    if (lv_video_ui.favi)
    {
        free(lv_video_ui.favi);
    }

    if (lv_video_ui.vfileinfo || lv_video_ui.pname || lv_video_ui.voffsettbl)
    {
        free(lv_video_ui.vfileinfo);
        free(lv_video_ui.pname);
        free(lv_video_ui.voffsettbl);
    }  

    lv_video_ui.video_main_ui = NULL;
	gpio_set_level(GPIO_NUM_11, 0);   /* 关闭喇叭 */
    printf("exit lv_video_del\n");
}

void lv_app_video_init(void)
{
    uint8_t res;
    uint32_t temp;
    static uint8_t video_init_flag = 0;
    if (lv_general.sd_check_en == SD_DISCONNECT)
    {
        lv_msgbox("SD device not detected");
        return;
    }
    else 
    {
        sdmmc_init();

        if (f_opendir(&lv_video_ui.vdir, "0:/VIDEO"))
        {
            lv_msgbox("VIDEO folder error");
            return ;
        }

        lv_video_ui.totavinum = video_get_tnum("0:/VIDEO");                         /* 得到总有效文件数 */
        lv_video_ui.vfileinfo = (FILINFO *)malloc(sizeof(FILINFO));                 /* 为长文件缓存区分配内存 */
        lv_video_ui.pname = (uint8_t *)malloc(2 * 255 + 1);                         /* 为带路径的文件名分配内存 */
        lv_video_ui.voffsettbl = (uint32_t *)malloc(lv_video_ui.totavinum  * 4);    /* 申请4*totavinum个字节的内存,用于存放视频文件索引 */
        
        if (!lv_video_ui.vfileinfo || !lv_video_ui.pname || !lv_video_ui.voffsettbl)
        {
            lv_msgbox("memory allocation failed");
            return ;
        }

        /* 记录索引 */    
        res = (uint8_t)f_opendir(&lv_video_ui.vdir, "0:/VIDEO");    /* 打开目录 */

        if (res == 0)
        {
            lv_video_ui.curindex = 0;   /* 当前索引为0 */
            
            while (1)       /* 全部查询一遍 */
            {
                temp = lv_video_ui.vdir.dptr;                               /* 记录当前dptr偏移 */
                res = (uint8_t)f_readdir(&lv_video_ui.vdir, lv_video_ui.vfileinfo);     /* 读取下一个文件 */
                if ((res != 0) || (lv_video_ui.vfileinfo->fname[0] == 0))   /* 错误或到末尾，退出 */
                {
                    break;
                }
                
                res = exfuns_file_type(lv_video_ui.vfileinfo->fname);       /* 检测文件类型 */

                if ((res & 0xF0) == 0x60)                                   /* 是视频文件 */
                {
                    lv_video_ui.voffsettbl[lv_video_ui.curindex] = temp;    /* 记录索引 */
                    lv_video_ui.curindex++;
                }
            }
        }

        lv_video_ui.curindex = 0;                                           /* 从0开始显示 */
        lv_obj_add_flag(lv_app_ui.main_cont,LV_OBJ_FLAG_HIDDEN);
        for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
        {
            lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
        }
        if (video_init_flag == 0)
        {
            /* 创建队列集 */
            video_xQueueSet = xQueueCreateSet(4);
            video_xSemaphore_next = xSemaphoreCreateBinary();
            video_xSemaphore_prev = xSemaphoreCreateBinary();
            video_xSemaphore_exit = xSemaphoreCreateBinary();
            video_xSemaphore_starst_pause = xSemaphoreCreateBinary();

            if (video_xQueueSet == NULL || video_xSemaphore_next == NULL || video_xSemaphore_prev == NULL || video_xSemaphore_exit == NULL)
            {
                return ;
            }
            
            xQueueAddToSet(video_xSemaphore_next, video_xQueueSet);
            xQueueAddToSet(video_xSemaphore_prev, video_xQueueSet);
            xQueueAddToSet(video_xSemaphore_exit, video_xQueueSet);
            xQueueAddToSet(video_xSemaphore_starst_pause, video_xQueueSet);

            video_init_flag = 1;
        }

        lv_video_ui.video_main_ui = lv_obj_create(lv_scr_act());
        lv_obj_set_style_bg_color(lv_video_ui.video_main_ui, lv_color_hex(0x000000), LV_STATE_DEFAULT);                  /* 设置背景颜色 */
        lv_obj_set_size(lv_video_ui.video_main_ui, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()) - lv_obj_get_height(lv_scr_act()) / 20);  /* 设置容器大小 */
        lv_obj_set_style_radius(lv_video_ui.video_main_ui, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
        lv_obj_set_style_border_opa(lv_video_ui.video_main_ui, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
        lv_obj_set_pos(lv_video_ui.video_main_ui, 0, lv_obj_get_height(lv_scr_act()) / 20);                              /* 设置位置 */
        lv_obj_clear_flag(lv_video_ui.video_main_ui, LV_OBJ_FLAG_SCROLLABLE);                                            /* 禁止滚动 */
        lv_obj_update_layout(lv_video_ui.video_main_ui);

        lv_video_ui.video_win_obj = lv_win_create(lv_video_ui.video_main_ui,lv_obj_get_height(lv_scr_act()) / 15);
        lv_obj_set_size(lv_video_ui.video_win_obj, lv_obj_get_width(lv_video_ui.video_main_ui), lv_obj_get_height(lv_video_ui.video_main_ui));  /* 设置容器大小 */
        lv_obj_center(lv_video_ui.video_win_obj);
        lv_video_ui.video_win_title = lv_win_add_title(lv_video_ui.video_win_obj, "WKS VIDEO");    /* 根据播放文件名设置标题栏 */
        lv_video_ui.video_win_header = lv_win_get_header(lv_video_ui.video_win_obj);
        lv_video_ui.video_win_content = lv_win_get_content(lv_video_ui.video_win_obj);

        lv_obj_set_style_text_font(lv_video_ui.video_win_title, &lv_font_montserrat_48, LV_STATE_DEFAULT);  /* 设置标题栏字体 */
        lv_obj_set_style_text_color(lv_video_ui.video_win_title, lv_color_white(), LV_STATE_DEFAULT);       /* 设置标题栏字体颜色 */
        lv_obj_set_style_text_align(lv_video_ui.video_win_title, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);   /* 设置标题栏字体居中 */

        lv_obj_set_style_bg_color(lv_video_ui.video_win_header, lv_color_make(38,39,41), LV_STATE_DEFAULT);     /* 设置标题栏背景色 */
        lv_obj_set_style_bg_color(lv_video_ui.video_win_content, lv_color_make(68,72,75), LV_STATE_DEFAULT);    /* 设置标题栏背景色 */
        lv_obj_clear_flag(lv_video_ui.video_win_content, LV_OBJ_FLAG_SCROLLABLE); /* 禁止滚动 */

        lv_video_ui.video_img_obj = lv_img_create(lv_video_ui.video_win_content);
        lv_obj_align(lv_video_ui.video_img_obj, LV_ALIGN_CENTER, 0, 0);/* 设置图片对齐方式 */
        lv_obj_set_style_bg_color(lv_video_ui.video_img_obj, lv_color_hex(0x000000), LV_STATE_DEFAULT);

        lv_video_ui.video_box_t.video_box = lv_obj_create(lv_video_ui.video_win_content);
        lv_obj_set_size(lv_video_ui.video_box_t.video_box, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()) / 14 + 30);
        lv_obj_align(lv_video_ui.video_box_t.video_box, LV_ALIGN_BOTTOM_MID, 0, 22);
        lv_obj_set_style_radius(lv_video_ui.video_box_t.video_box, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
        lv_obj_set_style_border_opa(lv_video_ui.video_box_t.video_box, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
        lv_obj_clear_flag(lv_video_ui.video_box_t.video_box, LV_OBJ_FLAG_SCROLLABLE);                                            /* 禁止滚动 */
        lv_obj_set_style_bg_color(lv_video_ui.video_box_t.video_box, lv_color_make(38,39,41), LV_STATE_DEFAULT);

        lv_video_ui.video_slider_obj = lv_slider_create(lv_video_ui.video_win_content);
        lv_slider_set_range(lv_video_ui.video_slider_obj,0,255);
        lv_obj_set_size(lv_video_ui.video_slider_obj, lv_obj_get_width(lv_scr_act()), 10);
        lv_obj_align(lv_video_ui.video_slider_obj, LV_ALIGN_BOTTOM_MID, 0, -lv_obj_get_height(lv_scr_act()) / 14 - 8);
        lv_obj_set_style_radius(lv_video_ui.video_slider_obj, 0, LV_PART_INDICATOR);
        lv_obj_set_style_radius(lv_video_ui.video_slider_obj, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(lv_video_ui.video_slider_obj, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(lv_video_ui.video_slider_obj, lv_color_hex(0xFF0000), LV_PART_KNOB);
        lv_obj_set_style_bg_color(lv_video_ui.video_slider_obj, lv_color_make(141,143,142), LV_STATE_DEFAULT);
        lv_slider_set_value(lv_video_ui.video_slider_obj,0, LV_ANIM_ON);
        lv_obj_clear_flag(lv_video_ui.video_slider_obj, LV_OBJ_FLAG_CLICKABLE); /* 禁止滚动 */

        lv_video_ui.video_box_t.video_start_pause = lv_label_create(lv_video_ui.video_box_t.video_box);
        lv_obj_set_ext_click_area(lv_video_ui.video_box_t.video_start_pause,50);
        lv_obj_set_style_text_color(lv_video_ui.video_box_t.video_start_pause,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lv_video_ui.video_box_t.video_start_pause, &lv_font_montserrat_48,LV_STATE_DEFAULT);
        lv_label_set_text(lv_video_ui.video_box_t.video_start_pause, LV_SYMBOL_PAUSE);
        lv_obj_align(lv_video_ui.video_box_t.video_start_pause,LV_ALIGN_CENTER, 0, 0);

        lv_video_ui.video_box_t.video_next = lv_label_create(lv_video_ui.video_box_t.video_box);
        lv_obj_set_ext_click_area(lv_video_ui.video_box_t.video_next,50);
        lv_obj_set_style_text_color(lv_video_ui.video_box_t.video_next,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lv_video_ui.video_box_t.video_next, &lv_font_montserrat_48,LV_STATE_DEFAULT);
        lv_label_set_text(lv_video_ui.video_box_t.video_next, LV_SYMBOL_NEXT);
        lv_obj_align_to(lv_video_ui.video_box_t.video_next, lv_video_ui.video_box_t.video_start_pause,LV_ALIGN_OUT_RIGHT_MID,  50, 0);

        lv_video_ui.video_box_t.video_prev = lv_label_create(lv_video_ui.video_box_t.video_box);
        lv_obj_set_ext_click_area(lv_video_ui.video_box_t.video_prev,50);
        lv_obj_set_style_text_color(lv_video_ui.video_box_t.video_prev,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lv_video_ui.video_box_t.video_prev, &lv_font_montserrat_48,LV_STATE_DEFAULT);
        lv_label_set_text(lv_video_ui.video_box_t.video_prev, LV_SYMBOL_PREV);
        lv_obj_align_to(lv_video_ui.video_box_t.video_prev, lv_video_ui.video_box_t.video_start_pause,LV_ALIGN_OUT_LEFT_MID,  -50, 0);

        lv_video_ui.video_box_t.video_volume = lv_label_create(lv_video_ui.video_box_t.video_box);
        lv_obj_set_style_text_color(lv_video_ui.video_box_t.video_volume,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lv_video_ui.video_box_t.video_volume, &lv_font_montserrat_48,LV_STATE_DEFAULT);
        lv_label_set_text(lv_video_ui.video_box_t.video_volume, LV_SYMBOL_VOLUME_MAX);
        lv_obj_align(lv_video_ui.video_box_t.video_volume,LV_ALIGN_LEFT_MID, 20, 0);

        lv_video_ui.video_box_t.video_volume_slider = lv_slider_create(lv_video_ui.video_box_t.video_box);
        lv_slider_set_range(lv_video_ui.video_box_t.video_volume_slider,0,33);
        lv_obj_set_size(lv_video_ui.video_box_t.video_volume_slider, 100, 10);
        lv_obj_set_style_radius(lv_video_ui.video_box_t.video_volume_slider, 0, LV_PART_INDICATOR);
        lv_obj_set_style_radius(lv_video_ui.video_box_t.video_volume_slider, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(lv_video_ui.video_box_t.video_volume_slider, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(lv_video_ui.video_box_t.video_volume_slider, lv_color_make(141,143,142), LV_STATE_DEFAULT);
        lv_obj_remove_style(lv_video_ui.video_box_t.video_volume_slider, NULL, LV_PART_KNOB);
        lv_obj_align_to(lv_video_ui.video_box_t.video_volume_slider, lv_video_ui.video_box_t.video_volume,LV_ALIGN_OUT_RIGHT_MID, 15, 0);
        lv_slider_set_value(lv_video_ui.video_box_t.video_volume_slider,20, LV_ANIM_ON);

        lv_video_ui.video_box_t.video_list = lv_label_create(lv_video_ui.video_box_t.video_box);
        lv_obj_set_style_text_color(lv_video_ui.video_box_t.video_list,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(lv_video_ui.video_box_t.video_list, &lv_font_montserrat_48,LV_STATE_DEFAULT);
        lv_label_set_text(lv_video_ui.video_box_t.video_list, LV_SYMBOL_LIST);
        lv_obj_align(lv_video_ui.video_box_t.video_list,LV_ALIGN_RIGHT_MID, -20, 0);

        /* 可点击状态 */
        lv_obj_add_flag(lv_video_ui.video_box_t.video_start_pause, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(lv_video_ui.video_box_t.video_next, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(lv_video_ui.video_box_t.video_prev, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(lv_video_ui.video_box_t.video_volume, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(lv_video_ui.video_box_t.video_list, LV_OBJ_FLAG_CLICKABLE);
        /* 添加回调函数 */
        lv_obj_add_event_cb(lv_video_ui.video_box_t.video_start_pause, lv_video_event_cb,LV_EVENT_ALL,NULL);
        lv_obj_add_event_cb(lv_video_ui.video_box_t.video_next, lv_video_event_cb,LV_EVENT_ALL,NULL);
        lv_obj_add_event_cb(lv_video_ui.video_box_t.video_prev, lv_video_event_cb,LV_EVENT_ALL,NULL);
        lv_obj_add_event_cb(lv_video_ui.video_box_t.video_volume, lv_video_event_cb,LV_EVENT_ALL,NULL);
        lv_obj_add_event_cb(lv_video_ui.video_box_t.video_list, lv_video_event_cb,LV_EVENT_ALL,NULL);
        lv_obj_add_event_cb(lv_video_ui.video_box_t.video_volume_slider, lv_video_event_cb,LV_EVENT_ALL,NULL);

        if (VIDEOTask_Handler == NULL)
        {
            xTaskCreatePinnedToCore((TaskFunction_t )lv_video,
                                    (const char*    )"lv_video",
                                    (uint16_t       )VIDEO_STK_SIZE,
                                    (void*          )NULL,
                                    (UBaseType_t    )VIDEO_PRIO,
                                    (TaskHandle_t*  )&VIDEOTask_Handler,
                                    (BaseType_t     ) 1);
        }

        lv_general.current_parent = lv_video_ui.video_main_ui;
        lv_general.del_function = lv_video_del;
    }
}