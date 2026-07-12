/**
 ******************************************************************************
 * @file        app_pic.c
 * @version     V1.0
 * @brief       相册 APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "app_pic.h"

pic_ui_t lv_pic_ui;
QueueSetHandle_t xQueueSet;                 /* 定义队列集 */
QueueHandle_t           xSemaphore_next;    /* 定义下一个图片开关信号量 */
SemaphoreHandle_t       xSemaphore_prev;    /* 定义上一个图片开关信号量 */

/* PIC Task Configuration
 * Including: task handle, task priority, stack size, creation task
 */
#define PIC_PRIO      10                                /* task priority */
#define PIC_STK_SIZE  5 * 1024                          /* task stack size */
TaskHandle_t          PICTask_Handler;                  /* task handle */
void pic(void *pvParameters);                           /* Task function */


/**
 * @brief       转换
 * @param       fs:文件系统对象
 * @param       clst:转换
 * @retval      =0:扇区号，0:失败
 */
static LBA_t atk_clst2sect(FATFS *fs, DWORD clst)
{
    clst -= 2;  /* Cluster number is origin from 2 */

    if (clst >= fs->n_fatent - 2)
    {
        return 0;   /* Is it invalid cluster number? */
    }

    return fs->database + (LBA_t)fs->csize * clst;  /* Start sector number of the cluster */
}

/**
 * @brief       偏移
 * @param       dp:指向目录对象
 * @param       Offset:目录表的偏移量
 * @retval      FR_OK(0):成功，!=0:错误
 */
FRESULT atk_dir_sdi(FF_DIR *dp, DWORD ofs)
{
    DWORD clst;
    FATFS *fs = dp->obj.fs;

    if (ofs >= (DWORD)((FF_FS_EXFAT && fs->fs_type == FS_EXFAT) ? 0x10000000 : 0x200000) || ofs % 32)
    {
        /* Check range of offset and alignment */
        return FR_INT_ERR;
    }

    dp->dptr = ofs;         /* Set current offset */
    clst = dp->obj.sclust;  /* Table start cluster (0:root) */

    if (clst == 0 && fs->fs_type >= FS_FAT32)
    {	/* Replace cluster# 0 with root cluster# */
        clst = (DWORD)fs->dirbase;

        if (FF_FS_EXFAT)
        {
            dp->obj.stat = 0;
        }
        /* exFAT: Root dir has an FAT chain */
    }

    if (clst == 0)
    {	/* Static table (root-directory on the FAT volume) */
        if (ofs / 32 >= fs->n_rootdir)
        {
            return FR_INT_ERR;  /* Is index out of range? */
        }

        dp->sect = fs->dirbase;

    }
    else
    {   /* Dynamic table (sub-directory or root-directory on the FAT32/exFAT volume) */
        dp->sect = atk_clst2sect(fs, clst);
    }

    dp->clust = clst;   /* Current cluster# */

    if (dp->sect == 0)
    {
        return FR_INT_ERR;
    }

    dp->sect += ofs / fs->ssize;             /* Sector# of the directory entry */
    dp->dir = fs->win + (ofs % fs->ssize);   /* Pointer to the entry in the win[] */

    return FR_OK;
}

/**
 * @brief       Obtain the total number of target files in the path path
 * @param       path : path
 * @retval      Total number of valid files
 */
uint16_t pic_get_tnum(char *path)
{
    uint8_t res;
    uint16_t rval = 0;
    FF_DIR tdir;
    FILINFO *tfileinfo;
    tfileinfo = (FILINFO *)malloc(sizeof(FILINFO));
    res = f_opendir(&tdir, (const TCHAR *)path);

    if (res == FR_OK && tfileinfo)
    {
        while (1)
        {
            res = f_readdir(&tdir, tfileinfo);

            if (res != FR_OK || tfileinfo->fname[0] == 0)break;
            res = exfuns_file_type(tfileinfo->fname);

            if ((res & 0X0F) != 0X00)
            {
                rval++;
            }
        }
    }

    free(tfileinfo);
    return rval;
}

lv_img_dsc_t img_pic_dsc = {
    .header.always_zero = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};

/**
 * @brief       PNG/BMPJPEG/JPG decoding
 * @param       filename : file name
 * @param       width    : image width
 * @param       height   : image height
 * @retval      None
 */
void lv_pic_png_bmp_jpeg_decode(uint16_t w,uint16_t h,uint8_t * pic_buf)
{
    /* 锁定互斥锁，因为LVGL API不是线程安全的 */
    if (lvgl_port_lock(0))
    {
        img_pic_dsc.header.w = w;
        img_pic_dsc.header.h = h;
        img_pic_dsc.data_size = w * h * 2;
        img_pic_dsc.data = (const uint8_t *)pic_buf;
        lv_img_set_src(lv_pic_ui.pic_frame,&img_pic_dsc);
        /* 释放互斥锁 */
        lvgl_port_unlock();  /* 释放互斥锁 */
    }
}

/**
 * @brief       pic task
 * @param       pvParameters : parameters (not used)
 * @retval      None
 */
void pic(void *pvParameters)
{
    pvParameters = pvParameters;
    QueueSetMemberHandle_t activate_member = NULL;
    uint8_t res = 0;
    uint16_t temp = 0;

    while(1)
    {
        res = f_opendir(&lv_pic_ui.picdir, "0:/PICTURE");

        if (res == FR_OK)
        {
            lv_pic_ui.pic_curindex = 0;

            while (1)
            {
                temp = lv_pic_ui.picdir.dptr;
                res = f_readdir(&lv_pic_ui.picdir, lv_pic_ui.pic_picfileinfo);
                if (res != FR_OK || lv_pic_ui.pic_picfileinfo->fname[0] == 0)break;

                res = exfuns_file_type(lv_pic_ui.pic_picfileinfo->fname);

                if ((res & 0X0F) != 0X00)
                {
                    lv_pic_ui.pic_picoffsettbl[lv_pic_ui.pic_curindex] = temp;
                    lv_pic_ui.pic_curindex++;
                }
            }
        }

        lv_pic_ui.pic_curindex = 0;
        res = f_opendir(&lv_pic_ui.picdir, (const TCHAR *)"0:/PICTURE");

        while (res == FR_OK)
        {
            atk_dir_sdi(&lv_pic_ui.picdir, lv_pic_ui.pic_picoffsettbl[lv_pic_ui.pic_curindex]);
            res = f_readdir(&lv_pic_ui.picdir, lv_pic_ui.pic_picfileinfo);

            if (res != FR_OK || lv_pic_ui.pic_picfileinfo->fname[0] == 0)break;

            strcpy((char *)lv_pic_ui.pic_pname, "0:/PICTURE/");
            strcat((char *)lv_pic_ui.pic_pname, (const char *)lv_pic_ui.pic_picfileinfo->fname);
            temp = exfuns_file_type(lv_pic_ui.pic_pname);
            switch (temp)
            {
                case T_JPG:
                case T_JPEG:
                    jpeg_decode(lv_pic_ui.pic_pname,0,0,lv_pic_png_bmp_jpeg_decode);   /* JPG/JPEG decode */
                    break;
                default:
                    xSemaphoreGive(xSemaphore_next);
                    break;
            }

            while (1)
            {
                activate_member = xQueueSelectFromSet(xQueueSet, 10);/* 等待队列集中的队列接收到消息 */

                if (activate_member == xSemaphore_prev)
                {
                    xSemaphoreTake(activate_member, 20);

                    if (lv_pic_ui.pic_curindex)
                    {
                        lv_pic_ui.pic_curindex--;
                    }
                    else
                    {
                        lv_pic_ui.pic_curindex = lv_pic_ui.pic_totpicnum - 1;
                    }

                    break;

                }
                else if (activate_member == xSemaphore_next)
                {
                    xSemaphoreTake(activate_member, 20);

                    lv_pic_ui.pic_curindex++;

                    if (lv_pic_ui.pic_curindex >= lv_pic_ui.pic_totpicnum)
                    {
                        lv_pic_ui.pic_curindex = 0;
                    }

                    break;
                }
            }
        }
    }
}

/**
  * @brief  del pic
  * @param  None
  * @retval None
  */
void lv_pic_del(void)
{
    if (PICTask_Handler != NULL)
    {
        vTaskDelete(PICTask_Handler);
        PICTask_Handler = NULL;
    }

    if (lv_pic_ui.pic_picfileinfo || lv_pic_ui.pic_pname || lv_pic_ui.pic_picoffsettbl)
    {
        free(lv_pic_ui.pic_picfileinfo);
        free(lv_pic_ui.pic_pname);
        free(lv_pic_ui.pic_picoffsettbl);
    }

    lv_pic_ui.pic_main_ui = NULL;
}

/* 函数声明 */
void lv_app_pic_init(void)
{
    static uint8_t pic_init_flag = 0;
    if (lv_general.sd_check_en == SD_DISCONNECT)
    {
        lv_msgbox("SD device not detected");
        return;
    }
    else 
    {
        sdmmc_init();

        if (f_opendir(&lv_pic_ui.picdir, "0:/PICTURE"))
        {
            lv_msgbox("PICTURE folder error");
            return ;
        }
        
        lv_pic_ui.pic_totpicnum = pic_get_tnum("0:/PICTURE");

        if (lv_pic_ui.pic_totpicnum == 0)
        {
            lv_msgbox("No pic files");
            return ;
        }

        lv_pic_ui.pic_picfileinfo = (FILINFO *)malloc(sizeof(FILINFO));
        lv_pic_ui.pic_pname = malloc(255 * 2 + 1);
        lv_pic_ui.pic_picoffsettbl = malloc(4 * lv_pic_ui.pic_totpicnum);

        if (!lv_pic_ui.pic_picfileinfo || !lv_pic_ui.pic_pname || !lv_pic_ui.pic_picoffsettbl)
        {
            lv_msgbox("memory allocation failed");
            return ;
        }

        if (pic_init_flag == 0)
        {
            /* 创建队列集 */
            xQueueSet = xQueueCreateSet(2);
            xSemaphore_next = xSemaphoreCreateBinary();
            xSemaphore_prev = xSemaphoreCreateBinary();

            if (xQueueSet == NULL || xSemaphore_next == NULL || xSemaphore_prev == NULL)
            {
                return ;
            }
            
            xQueueAddToSet(xSemaphore_next, xQueueSet);
            xQueueAddToSet(xSemaphore_prev, xQueueSet);

            pic_init_flag = 1;
        }

        lv_obj_add_flag(lv_app_ui.main_cont,LV_OBJ_FLAG_HIDDEN);
        for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
        {
            lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index],LV_OBJ_FLAG_CLICKABLE);
        }

        lv_pic_ui.pic_main_ui = lv_obj_create(lv_scr_act());
        lv_obj_set_style_bg_color(lv_pic_ui.pic_main_ui, lv_color_hex(0x000000), LV_STATE_DEFAULT);                  /* 设置背景颜色 */
        lv_obj_set_size(lv_pic_ui.pic_main_ui, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()) - lv_obj_get_height(lv_scr_act()) / 20);  /* 设置容器大小 */
        lv_obj_set_style_radius(lv_pic_ui.pic_main_ui, 0, LV_STATE_DEFAULT);                                         /* 无圆角 */
        lv_obj_set_style_border_opa(lv_pic_ui.pic_main_ui, LV_OPA_0, LV_STATE_DEFAULT);                              /* 边界透明 */
        lv_obj_set_pos(lv_pic_ui.pic_main_ui, 0, lv_obj_get_height(lv_scr_act()) / 20);                              /* 设置位置 */
        lv_obj_clear_flag(lv_pic_ui.pic_main_ui, LV_OBJ_FLAG_SCROLLABLE);                                            /* 禁止滚动 */
        lv_obj_update_layout(lv_pic_ui.pic_main_ui);

        lv_pic_ui.pic_frame = lv_img_create(lv_pic_ui.pic_main_ui);
        lv_obj_set_style_bg_color(lv_pic_ui.pic_frame, lv_color_hex(0x000000), LV_STATE_DEFAULT);
        lv_obj_align(lv_pic_ui.pic_frame,LV_ALIGN_CENTER,0,0);

        lv_general.current_parent = lv_pic_ui.pic_main_ui;
        lv_general.del_function = lv_pic_del;

        if (PICTask_Handler == NULL)
        {
            xTaskCreatePinnedToCore((TaskFunction_t )pic,
                                    (const char*    )"pic",
                                    (uint16_t       )PIC_STK_SIZE,
                                    (void*          )NULL,
                                    (UBaseType_t    )PIC_PRIO,
                                    (TaskHandle_t*  )&PICTask_Handler,
                                    (BaseType_t     ) 1);
        }
    }
}