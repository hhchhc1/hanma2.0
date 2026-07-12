/**
 ******************************************************************************
 * @file        app_music.c
 * @version     V1.0
 * @brief       音乐 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_music.h"
#include "myi2s.h"

QueueSetHandle_t        music_xQueueSet;            /* 定义队列集 */
QueueHandle_t           music_xSemaphore_next;      /* 下一首歌曲 */
SemaphoreHandle_t       music_xSemaphore_prev;      /* 上一首歌曲 */
SemaphoreHandle_t       music_xSemaphore_starst_pause;  /* 暂停和开启 */
SemaphoreHandle_t       music_xSemaphore_exit;  /* 退出界面 */

music_ui_t lv_music_ui;
LV_IMG_DECLARE(ui_img_album_png);

__wavctrl wavctrl;
__audiodev g_audiodev;
UINT bytes_write = 0;                       /* 写一次I2S大小 */
volatile long long int i2s_table_size = 0;  /* 积累每次发送音频数据总大小 */
esp_err_t i2s_play_end = ESP_FAIL;          /* 播放结束标志位 */
esp_err_t i2s_play_next_prev = ESP_FAIL;    /* 下一首或者上一首标志位 */
FSIZE_t file_read_pos = 0;                  /* 记录当前WAV读取位置 */
uint8_t key = MUSIC_NULL;

/* MUSIC Task Configuration
 * Including: task handle, task priority, stack size, creation task
 */
#define MUSIC_PRIO      10                   /* task priority */
#define MUSIC_STK_SIZE  5*1024              /* task stack size */
TaskHandle_t            MUSICTask_Handler;  /* task handle */
void app_music(void *pvParameters);             /* Task function */

/* PLAY Task Configuration
 * Including: task handle, task priority, stack size, creation task
 */
#define PLAY_PRIO      10                  /* task priority */
#define PLAY_STK_SIZE  5*1024              /* task stack size */
TaskHandle_t           PLAYTask_Handler;   /* task handle */
void play(void *pvParameters);             /* Task function */

/**
 * @brief       Start audio playback
 * @param       None
 * @retval      None
 */
void lv_audio_start(void)
{
    g_audiodev.status = 3 << 0;
    i2s_trx_start();
}

/**
 * @brief       Stop audio playback
 * @param       None
 * @retval      None
 */
void lv_audio_stop(void)
{
    g_audiodev.status = 0;
    i2s_trx_stop();
}

/**
 * @brief       Display playback time and bit rate information
 * @param       totsec : Total time length of audio files
 * @param       cursec : Current playback time
 * @param       bitrate: Bit rate
 * @retval      None
 */
void audio_msg_show(uint32_t totsec, uint32_t cursec, uint32_t bitrate)
{
    static uint16_t playtime = 0xFFFF;
    
    if (playtime != cursec)
    {
        playtime = cursec;
        lv_arc_set_range(lv_music_ui.music_arc_center, 0, totsec);  /* 设置 arc 的范围 */
        lv_arc_set_value(lv_music_ui.music_arc_center, playtime);   /* 设置 arc 的值 */
    }
}

/**
 * @brief       WAV file parsing
 * @param       fname : File path
 * @param       wavx  : Structure pointer
 * @retval      0:Open file successfully
 *              1:fail to open file
 *              2:Non WAV files
 *              3:DATA region not found
 */
uint8_t wav_decode_init(uint8_t *fname, __wavctrl *wavx)
{
    FIL *ftemp;
    uint8_t *buf; 
    uint32_t br = 0;
    uint8_t res = 0;

    ChunkRIFF *riff;
    ChunkFMT *fmt;
    ChunkFACT *fact;
    ChunkDATA *data;
    
    ftemp = (FIL*)malloc(sizeof(FIL));
    buf = malloc(512);
    
    if (ftemp && buf)
    {
        res = f_open(ftemp, (TCHAR*)fname, FA_READ);            /* open file */
        
        if (res == FR_OK)
        {
            f_read(ftemp, buf, 512, (UINT *)&br);               /* read 512 byte */
            riff = (ChunkRIFF *)buf;                            /* if it is a WAV file */
            
            if (riff->Format == 0x45564157)
            {
                fmt = (ChunkFMT *)(buf + 12);
                fact = (ChunkFACT *)(buf + 12 + 8 + fmt->ChunkSize);
                
                if (fact->ChunkID == 0x74636166 || fact->ChunkID == 0x5453494C)
                {
                    wavx->datastart = 12 + 8 + fmt->ChunkSize + 8 + fact->ChunkSize;
                }
                else
                {
                    wavx->datastart = 12 + 8 + fmt->ChunkSize;
                }
                
                data = (ChunkDATA *)(buf + wavx->datastart);
                
                if (data->ChunkID == 0x61746164)                /* parsed successfully */
                {
                    wavx->audioformat = fmt->AudioFormat;       /* audio Format */
                    wavx->nchannels = fmt->NumOfChannels;       /* number of channels */
                    wavx->samplerate = fmt->SampleRate;         /* sampling rate */
                    wavx->bitrate = fmt->ByteRate * 8;
                    wavx->blockalign = fmt->BlockAlign;
                    wavx->bps = fmt->BitsPerSample;
                    
                    wavx->datasize = data->ChunkSize;
                    wavx->datastart = wavx->datastart + 8;
                     
                    printf("wavx->audioformat:%d\r\n", wavx->audioformat);
                    printf("wavx->nchannels:%d\r\n", wavx->nchannels);
                    printf("wavx->samplerate:%ld\r\n", wavx->samplerate);
                    printf("wavx->bitrate:%ld\r\n", wavx->bitrate);
                    printf("wavx->blockalign:%d\r\n", wavx->blockalign);
                    printf("wavx->bps:%d\r\n", wavx->bps);
                    printf("wavx->datasize:%ld\r\n", wavx->datasize);
                    printf("wavx->datastart:%ld\r\n", wavx->datastart);  
                }
                else
                {
                    res = 3;
                }
            }
            else
            {
                res = 2;
            }
        }
        else
        {
            res = 1;
        }
    }
    
    f_close(ftemp);
    free(ftemp);
    free(buf); 
    
    return 0;
}

/**
 * @brief       Get the current playback time
 * @param       fx    : field name pointer
 * @param       wavx  : Structure pointer
 * @retval      None
 */
void wav_get_curtime(FIL *fx, __wavctrl *wavx)
{
    long long fpos;

    wavx->totsec = wavx->datasize / (wavx->bitrate / 8);
    fpos = fx->fptr-wavx->datastart;
    wavx->cursec = fpos * wavx->totsec / wavx->datasize;
}


/**
 * @brief       Obtain the total number of target files in the path path
 * @param       path : path
 * @retval      Total number of valid files
 */
uint16_t audio_get_tnum(uint8_t *path)
{
    uint8_t res;
    uint16_t rval = 0;
    FF_DIR tdir;
    FILINFO *tfileinfo;
    
    tfileinfo = (FILINFO*)malloc(sizeof(FILINFO));
    
    res = f_opendir(&tdir, (const TCHAR*)path);
    
    if ((res == FR_OK) && tfileinfo)
    {
        while (1)
        {
            res = f_readdir(&tdir, tfileinfo);
            
            if ((res != FR_OK) || (tfileinfo->fname[0] == 0))
            {
                break;
            }

            res = exfuns_file_type(tfileinfo->fname);
            
            if ((res & 0xF0) == 0x40)
            {
                rval++; /* 有效文件数增加1 */
            }
        }
    }
    
    free(tfileinfo);
    
    return rval;
}

/**
 * @brief       play任务
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void play(void *pvParameters)
{
    pvParameters = pvParameters;
    file_read_pos = 0;
    i2s_table_size = 0; 
	
	gpio_set_level(GPIO_NUM_11, 1);   /* 打开喇叭 */
    vTaskDelay(pdMS_TO_TICKS(20));
    i2s_tx_write(g_audiodev.tbuf, WAV_TX_BUFSIZE);  /* 先发送一段无声音的数据 */

    while(1)
    {
        if ((g_audiodev.status & 0x0F) == 0x03)     /* 打开了音频 */
        {
            for(uint16_t readTimes = 0; readTimes < (wavctrl.datasize / WAV_TX_BUFSIZE); readTimes++)
            {
                if ((g_audiodev.status & 0x0F) == 0x00)             /* 暂停播放 */
                {
                    file_read_pos = f_tell(g_audiodev.file);        /* 记录暂停位置 */

                    while(1)
                    {
                        if ((g_audiodev.status & 0x0F) == 0x03)     /* 重新打开了 */
                        {
                            break;
                        }

                        vTaskDelay(pdMS_TO_TICKS(5));               /* 死等 */
                    }

                    f_lseek(g_audiodev.file, file_read_pos);        /* 跳过到之前停止的位置 */
                }

                /* 判断是否播放完成 */
                if (i2s_table_size >= wavctrl.datasize || i2s_play_next_prev == ESP_OK)
                {
                    lv_audio_stop();                /* 先停止播放 */
                    i2s_table_size = 0;             /* 总大小清零 */
                    i2s_play_end = ESP_OK;          /* 已播放完成标志位 */
                    vTaskDelete(NULL);              /* 删除当前任务 */
                    vTaskDelay(pdMS_TO_TICKS(5));   /* 适当延时（为了删除这个任务） */
                    break;                          /* 防止延时5ms未能删除音频任务 */
                }

                f_read(g_audiodev.file,g_audiodev.tbuf, WAV_TX_BUFSIZE, (UINT*)&bytes_write);
                i2s_table_size = i2s_table_size + i2s_tx_write(g_audiodev.tbuf, WAV_TX_BUFSIZE);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    vTaskDelete(NULL);
}

/**
 * @brief  Music playback event callback
 * @param  *e ：event
 * @return None
 */
static void song_play_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    if (target == lv_music_ui.music_start_pause)        /* Start/Stop */
    {
        if (code == LV_EVENT_PRESSED)
        {
            xSemaphoreGive(music_xSemaphore_starst_pause);
        }
    }
    else if (target == lv_music_ui.music_next)       /* Next Song */
    {
        if (code == LV_EVENT_PRESSED)
        {
            xSemaphoreGive(music_xSemaphore_next);
        }
    }
    else if (target == lv_music_ui.music_prev)       /* Previous Song */
    {
        if (code == LV_EVENT_PRESSED)
        {
            xSemaphoreGive(music_xSemaphore_prev);
        }
    }
}

void app_music(void *pvParameters)
{
    pvParameters = pvParameters;
    QueueSetMemberHandle_t music_activate_member = NULL;
    i2s_play_end = ESP_FAIL;
    i2s_play_next_prev = ESP_FAIL;
    uint8_t res;
    uint32_t temp;
    g_audiodev.file = (FIL*)malloc(sizeof(FIL));
    g_audiodev.tbuf = malloc(WAV_TX_BUFSIZE);
    // myi2s_init();

    if (g_audiodev.file == NULL || g_audiodev.tbuf == NULL)
    {
        ESP_LOGE("TAG", "malloc failed");
        vTaskDelete(NULL);
    }

    while(1)
    {
        res = f_opendir(&lv_music_ui.wavdir, "0:/MUSIC"); 

        if(res == FR_OK)
        {
            lv_music_ui.curindex = 0;                                           /* the current index is 0 */
            
            while (1)
            {
                temp = lv_music_ui.wavdir.dptr;                                 /* record current index */

                res = f_readdir(&lv_music_ui.wavdir, lv_music_ui.wavfileinfo);  /* read a file from the directory */
                
                if ((res != FR_OK) || (lv_music_ui.wavfileinfo->fname[0] == 0))
                {
                    break;                                                      /* error/at the end, exit */
                }

                res = exfuns_file_type(lv_music_ui.wavfileinfo->fname);
                
                if ((res & 0xF0) == 0x40)
                {
                    lv_music_ui.wavoffsettbl[lv_music_ui.curindex] = temp;      /* record index */
                    lv_music_ui.curindex++;
                }
            }
        }

        lv_music_ui.curindex = 0;       
        res = f_opendir(&lv_music_ui.wavdir, (const TCHAR*)"0:/MUSIC");         /* display from 0 */

        while (res == FR_OK)                                                    /* open successfully */
        {
            lv_obj_remove_event_cb(lv_scr_act(),lv_scr_event_cb); /* 移除屏幕事件回调 */
            atk_dir_sdi(&lv_music_ui.wavdir, lv_music_ui.wavoffsettbl[lv_music_ui.curindex]);   /* change the current directory index */
            res = f_readdir(&lv_music_ui.wavdir, lv_music_ui.wavfileinfo);                      /* read a file from the directory */
            
            if ((res != FR_OK) || (lv_music_ui.wavfileinfo->fname[0] == 0))
            {
                break;  /* error/at the end, exit */
            }

            strcpy((char *)lv_music_ui.pname, "0:/MUSIC/");
            strcat((char *)lv_music_ui.pname, (const char *)lv_music_ui.wavfileinfo->fname);
            if (g_audiodev.file || g_audiodev.tbuf)
            {
                memset(g_audiodev.file,0,sizeof(FIL));                  /* 文件指针清零 */
                memset(g_audiodev.tbuf,0,WAV_TX_BUFSIZE);               /* buf清零 */
                memset(&wavctrl,0,sizeof(__wavctrl));                   /* 对WAV结构体相关参数清零 */

                res = wav_decode_init(lv_music_ui.pname, &wavctrl);     /* previous song */

                if (res == 0)                                           /* 解码成功 */
                {
                    if (wavctrl.bps == 16)                              /* 根据解码文件重新配置采样率和位宽 */
                    {
                        i2s_set_samplerate_bits_sample(wavctrl.samplerate,I2S_BITS_PER_SAMPLE_16BIT);
                        lv_audio_start();  /* 开启I2S */
                    }
                    else if (wavctrl.bps == 24)
                    {
                        i2s_set_samplerate_bits_sample(wavctrl.samplerate,I2S_BITS_PER_SAMPLE_24BIT);
                        lv_audio_start();  /* 开启I2S */
                    }
                    else
                    {
                        lv_audio_start();  /* 开启I2S */
                    }

                    res = f_open(g_audiodev.file, (TCHAR*)lv_music_ui.pname, FA_READ);      /* 打开WAV音频文件 */

                    if (res == FR_OK)                      /* 打开成功 */
                    {
                        
                        /* 打开成功后，才创建音频任务 */
                        if (PLAYTask_Handler == NULL && res == FR_OK)
                        {
                            xTaskCreate(play, "play", PLAY_STK_SIZE,NULL , PLAY_PRIO, &PLAYTask_Handler);
                        }
                    }

                    lv_obj_add_event_cb(lv_scr_act(), lv_scr_event_cb, LV_EVENT_ALL, NULL);

                    while (res == FR_OK)
                    { 
                        while (1)
                        {
                            /* 播放结束，下一首 */
                            if (i2s_play_end == ESP_OK)
                            {
                                key = MUSIC_NEXT;
                                break;
                            }

                            music_activate_member = xQueueSelectFromSet(music_xQueueSet, 10);/* 等待队列集中的队列接收到消息 */

                            if (music_activate_member == music_xSemaphore_next)
                            {
                                xSemaphoreTake(music_activate_member, 20);
                                i2s_play_next_prev = ESP_OK;                    /* 下一首 */
                                key = MUSIC_NEXT;
                            }
                            else if (music_activate_member == music_xSemaphore_prev)
                            {
                                xSemaphoreTake(music_activate_member, 20);
                                i2s_play_next_prev = ESP_OK;                    /* 上一首 */
                                key = MUSIC_PREV;
                            }
                            else if (music_activate_member == music_xSemaphore_starst_pause)
                            {
                                xSemaphoreTake(music_activate_member, 20);
                                if ((g_audiodev.status & 0x0F) == 0x03)
                                {
                                    lv_audio_stop();
                                    key = MUSIC_PAUSE;
                                    lv_label_set_text(lv_music_ui.song_play_label, LV_SYMBOL_PLAY);
                                }
                                else if ((g_audiodev.status & 0x0F) == 0x00)
                                {
                                    lv_audio_start();
                                    key = MUSIC_PLAY;
                                    lv_label_set_text(lv_music_ui.song_play_label, LV_SYMBOL_PAUSE);
                                }
                            }
                            else if (music_activate_member == music_xSemaphore_exit)
                            {
                                xSemaphoreTake(music_activate_member, 20);
                                g_audiodev.status = 0;
                                
                                while(1)
                                {
                                    vTaskDelay(pdMS_TO_TICKS(10));
                                }
                            }

                            if ((g_audiodev.status & 0x0F) == 0x03)                 /* 暂停不刷新时间 */
                            {
                                wav_get_curtime(g_audiodev.file, &wavctrl);         /* 得到总时间和当前播放的时间 */
                                audio_msg_show(wavctrl.totsec, wavctrl.cursec, wavctrl.bitrate);
                            }

                            vTaskDelay(pdMS_TO_TICKS(10));
                        }

                        if (key == MUSIC_NEXT || key == MUSIC_PREV)                   /* 退出切换歌曲 */
                        {
                            break;
                        }
                    }
                }
            }

            if (key == MUSIC_PREV)       /* 上一首 */
            {
                if (lv_music_ui.curindex)
                {
                    lv_music_ui.curindex--;
                }
                else
                {
                    lv_music_ui.curindex = lv_music_ui.totwavnum - 1;
                }
            }
            else if (key == MUSIC_NEXT)  /* 下一首 */
            {
                lv_music_ui.curindex++;

                if (lv_music_ui.curindex >= lv_music_ui.totwavnum)
                {
                    lv_music_ui.curindex = 0;
                }
            }

            lv_audio_start();           /* 开启I2S */
            PLAYTask_Handler = NULL;    /* 重新初始化播放任务 */
            i2s_play_end = ESP_FAIL;    /* 重新初始化播放结束标志 */
            i2s_play_next_prev = ESP_FAIL; /* 重新初始化播放下一首/上一首标志 */
            key = MUSIC_NULL;           /* 重新初始化按键标志 */
        }
    }
}

/**
  * @brief  del music
  * @param  None
  * @retval None
  */
void lv_music_del(void)
{
    // if ((g_audiodev.status & 0x0F) == 0x03)
    // {
        // lv_audio_stop();
    // }

	if(key == MUSIC_PAUSE)
	{
		lv_audio_start();
	}

    xSemaphoreGive(music_xSemaphore_exit);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // i2s_deinit();
    
    if (PLAYTask_Handler)
    {
        vTaskDelete(PLAYTask_Handler);
        PLAYTask_Handler = NULL;
    }
    
    if (MUSICTask_Handler)
    {
        vTaskDelete(MUSICTask_Handler);
        MUSICTask_Handler = NULL;    
    }

    f_close(g_audiodev.file);

    if (g_audiodev.file || g_audiodev.tbuf)
    {
        free(g_audiodev.file);
        free(g_audiodev.tbuf);
    }

    if (lv_music_ui.wavfileinfo || lv_music_ui.pname || lv_music_ui.wavoffsettbl)
    {
        free(lv_music_ui.wavfileinfo);
        free(lv_music_ui.pname);
        free(lv_music_ui.wavoffsettbl);
    }

    lv_music_ui.music_main_ui = NULL;
	gpio_set_level(GPIO_NUM_11, 0);   /* 关闭喇叭 */
    printf("exit lv_music_del\n");
}

void lv_app_music_init(void)
{
    static uint8_t music_init_flag = 0;

    if (lv_general.sd_check_en == SD_DISCONNECT)
    {
        lv_msgbox("SD device not detected");
        return;
    }
    else 
    {
        sdmmc_init();

        if (f_opendir(&lv_music_ui.wavdir, "0:/MUSIC"))
        {
            lv_msgbox("PICTURE folder error");
            return ;
        }
        
        lv_music_ui.totwavnum = audio_get_tnum((uint8_t *)"0:/MUSIC");
        printf("totwavnum = %d\n", lv_music_ui.totwavnum);
        if (lv_music_ui.totwavnum == 0)
        {
            lv_msgbox("No pic files");
            return ;
        }

        lv_music_ui.wavfileinfo = (FILINFO*)malloc(sizeof(FILINFO));
        lv_music_ui.pname = malloc(255 * 2 + 1);
        lv_music_ui.wavoffsettbl = malloc(4 * lv_music_ui.totwavnum);

        if (!lv_music_ui.wavfileinfo || !lv_music_ui.pname || !lv_music_ui.wavoffsettbl)
        {
            lv_msgbox("memory allocation failed");
            return ;
        }

        lv_obj_add_flag(lv_app_ui.main_cont,LV_OBJ_FLAG_HIDDEN);
        for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
        {
            lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
        }
        
        if (music_init_flag == 0)
        {
            /* 创建队列集 */
            music_xQueueSet = xQueueCreateSet(4);
            music_xSemaphore_next = xSemaphoreCreateBinary();
            music_xSemaphore_prev = xSemaphoreCreateBinary();
            music_xSemaphore_starst_pause = xSemaphoreCreateBinary();
            music_xSemaphore_exit = xSemaphoreCreateBinary();

            if (music_xQueueSet == NULL || music_xSemaphore_next == NULL || music_xSemaphore_prev == NULL || music_xSemaphore_starst_pause == NULL || music_xSemaphore_exit == NULL)
            {
                return ;
            }
            
            xQueueAddToSet(music_xSemaphore_next, music_xQueueSet);
            xQueueAddToSet(music_xSemaphore_prev, music_xQueueSet);
            xQueueAddToSet(music_xSemaphore_starst_pause, music_xQueueSet);
            xQueueAddToSet(music_xSemaphore_exit, music_xQueueSet);

            music_init_flag = 1;
        }

        lv_music_ui.music_main_ui = lv_obj_create(lv_scr_act());
        lv_obj_set_style_bg_color(lv_music_ui.music_main_ui, lv_color_hex(0x000000), LV_STATE_DEFAULT);                  /* 设置背景颜色 */
        lv_obj_set_size(lv_music_ui.music_main_ui, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()) - lv_obj_get_height(lv_scr_act()) / 20);  /* 设置容器大小 */
        lv_obj_set_style_radius(lv_music_ui.music_main_ui, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
        lv_obj_set_style_border_opa(lv_music_ui.music_main_ui, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
        lv_obj_set_pos(lv_music_ui.music_main_ui, 0, lv_obj_get_height(lv_scr_act()) / 20);                              /* 设置位置 */
        lv_obj_clear_flag(lv_music_ui.music_main_ui, LV_OBJ_FLAG_SCROLLABLE);                                            /* 禁止滚动 */
        lv_obj_update_layout(lv_music_ui.music_main_ui);

        lv_music_ui.music_img = lv_img_create(lv_music_ui.music_main_ui);
        lv_obj_set_size(lv_music_ui.music_img,ui_img_album_png.header.w, ui_img_album_png.header.h);
        lv_img_set_src(lv_music_ui.music_img,&ui_img_album_png);
        lv_obj_align(lv_music_ui.music_img,LV_ALIGN_TOP_MID,0,20);
        lv_obj_update_layout(lv_music_ui.music_img);

        lv_music_ui.music_arc_center = lv_arc_create(lv_music_ui.music_main_ui);
        lv_obj_set_size(lv_music_ui.music_arc_center,ui_img_album_png.header.w + 2, ui_img_album_png.header.h + 2);
        lv_obj_align_to(lv_music_ui.music_arc_center,lv_music_ui.music_img,LV_ALIGN_CENTER,0,0);
        lv_arc_set_bg_angles(lv_music_ui.music_arc_center, 0, 360);
        lv_arc_set_rotation(lv_music_ui.music_arc_center, 360);
        lv_obj_remove_style(lv_music_ui.music_arc_center, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_color(lv_music_ui.music_arc_center, lv_color_hex(0x0000FF), LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(lv_music_ui.music_arc_center, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_arc_set_range(lv_music_ui.music_arc_center, 0, 100);     /* 设置 arc 的范围 */
        lv_arc_set_value(lv_music_ui.music_arc_center, 0);          /* 设置 arc 的值 */
        lv_obj_clear_flag(lv_music_ui.music_arc_center, LV_OBJ_FLAG_CLICKABLE);

        lv_music_ui.music_start_pause = lv_obj_create(lv_music_ui.music_main_ui);
        lv_obj_set_size(lv_music_ui.music_start_pause, 100, 100);
        lv_obj_set_style_bg_color(lv_music_ui.music_start_pause, lv_color_make(20,20,20), LV_STATE_DEFAULT); 
        lv_obj_align_to(lv_music_ui.music_start_pause, lv_music_ui.music_arc_center,LV_ALIGN_OUT_BOTTOM_MID, 0, 50);
        lv_obj_set_style_border_color(lv_music_ui.music_start_pause,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(lv_music_ui.music_start_pause,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_translate_y(lv_music_ui.music_start_pause, 5, LV_STATE_PRESSED);
        lv_obj_set_style_radius(lv_music_ui.music_start_pause, 200, 0);
        lv_obj_set_style_border_width(lv_music_ui.music_start_pause,2,LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(lv_music_ui.music_start_pause,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_shadow_width(lv_music_ui.music_start_pause,10,LV_STATE_FOCUS_KEY);
        lv_obj_add_event_cb(lv_music_ui.music_start_pause, song_play_event_cb, LV_EVENT_ALL, NULL);
        
        lv_music_ui.song_play_label = lv_label_create(lv_music_ui.music_start_pause);
        lv_obj_set_style_text_font(lv_music_ui.song_play_label, &lv_font_montserrat_48, 0);
        lv_label_set_text(lv_music_ui.song_play_label, LV_SYMBOL_PAUSE);
        lv_obj_set_style_text_color(lv_music_ui.song_play_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(lv_music_ui.song_play_label, LV_ALIGN_CENTER, 0, 0);

        lv_music_ui.music_prev = lv_obj_create(lv_music_ui.music_main_ui);
        lv_obj_set_size(lv_music_ui.music_prev, 80, 80);
        lv_obj_align_to(lv_music_ui.music_prev, lv_music_ui.music_start_pause, LV_ALIGN_OUT_LEFT_MID, -20, 0);
        lv_obj_set_style_bg_color(lv_music_ui.music_prev,lv_color_make(20,20,20),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(lv_music_ui.music_prev,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(lv_music_ui.music_prev,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_translate_y(lv_music_ui.music_prev, 5, LV_STATE_PRESSED);
        lv_obj_set_style_radius(lv_music_ui.music_prev, 80, 0);
        lv_obj_set_style_border_width(lv_music_ui.music_prev,2,LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(lv_music_ui.music_prev,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_shadow_width(lv_music_ui.music_prev,10,LV_STATE_FOCUS_KEY);
        lv_obj_add_event_cb(lv_music_ui.music_prev, song_play_event_cb, LV_EVENT_ALL, NULL);
        
        lv_music_ui.song_last_label = lv_label_create(lv_music_ui.music_prev);
        lv_obj_set_style_text_font(lv_music_ui.song_last_label, &lv_font_montserrat_48, 0);
        lv_label_set_text(lv_music_ui.song_last_label, LV_SYMBOL_PREV);
        lv_obj_set_style_text_color(lv_music_ui.song_last_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(lv_music_ui.song_last_label, LV_ALIGN_CENTER, 0, 0);

        lv_music_ui.music_next = lv_obj_create(lv_music_ui.music_main_ui);
        lv_obj_set_size(lv_music_ui.music_next, 80, 80);
        lv_obj_align_to(lv_music_ui.music_next, lv_music_ui.music_start_pause, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
        lv_obj_set_style_bg_color(lv_music_ui.music_next,lv_color_make(20,20,20),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(lv_music_ui.music_next,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(lv_music_ui.music_next,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_translate_y(lv_music_ui.music_next, 5, LV_STATE_PRESSED);
        lv_obj_set_style_radius(lv_music_ui.music_next, 80, 0);
        lv_obj_set_style_border_width(lv_music_ui.music_next,2,LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(lv_music_ui.music_next,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_shadow_width(lv_music_ui.music_next,10,LV_STATE_FOCUS_KEY);
        lv_obj_add_event_cb(lv_music_ui.music_next, song_play_event_cb, LV_EVENT_ALL, NULL);
        
        lv_music_ui.song_next_label = lv_label_create(lv_music_ui.music_next);
        lv_obj_set_style_text_font(lv_music_ui.song_next_label, &lv_font_montserrat_48, 0);
        lv_label_set_text(lv_music_ui.song_next_label, LV_SYMBOL_NEXT);
        lv_obj_set_style_text_color(lv_music_ui.song_next_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(lv_music_ui.song_next_label, LV_ALIGN_CENTER, 0, 0);
        
        if (MUSICTask_Handler == NULL)
        {
            xTaskCreatePinnedToCore(app_music, "app_music", MUSIC_STK_SIZE, NULL, MUSIC_PRIO, &MUSICTask_Handler,1);
        }
        
        lv_general.current_parent = lv_music_ui.music_main_ui;
        lv_general.del_function = lv_music_del;
    }
}