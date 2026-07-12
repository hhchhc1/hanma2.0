/**
 ******************************************************************************
 * @file        mipi_cam.c
 * @version     V1.0
 * @brief       mipicamera驱动代码
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 * 
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "mipi_cam.h"
#include "mipi_lcd.h"

static const char *mipi_cam_tag = "mipi_cam";

#define FPS_FUNC_ON   1         /* 帧率显示开关 */
#define ALIGN_UP_BY(num, align) (((num) + ((align) - 1)) & ~((align) - 1))  /* 对齐操作 */

static TaskHandle_t lcd_cam_task_handle = NULL;
bool camera_running;
static uint8_t camera_init_flag = 0;

static void lcd_cam_task(void *arg);

// 如果没有 app_video_close 函数，需要声明或定义
// 如果确实没有这个函数，可以注释掉相关调用
int app_video_close(int fd) {
    // 简单的关闭实现，根据实际情况修改
    return close(fd);
}

/**
 * @brief       mipi_cam初始化
 * @param       无
 * @retval      ESP_OK:开启成功; ESP_FAIL:开启失败
 */
esp_err_t mipi_cam_init(void)
{
    esp_err_t ret = ESP_OK;
    mipi_dev_bsp_enable_dsi_phy_power();    /* 配置MIPI设备电源 */

    if(camera_init_flag == 0)
	{
		camera_init_flag = 1;
		
		ret = app_video_main(i2c_bus_handle);       /* 初始化摄像头硬件和软件接口 */
		if (ret != ESP_OK)
		{
			ESP_LOGE(mipi_cam_tag, "video main init failed with error 0x%x", ret);
			return ESP_FAIL;
		}
	}

    int video_cam_fd0 = app_video_open(0);  /* 打开摄像头设备 */
    if (video_cam_fd0 < 0)
    {
        ESP_LOGE(mipi_cam_tag, "video cam open failed");
        return ESP_FAIL;
    }

    ret = app_video_init(video_cam_fd0, APP_VIDEO_FMT_RGB565);  /* 初始化设备并设置捕获视频格式 */
    if (ret != ESP_OK)
    {
        ESP_LOGE(mipi_cam_tag, "Video cam init failed with error 0x%x", ret);
        return ESP_FAIL;
    }

    xTaskCreatePinnedToCore(lcd_cam_task, "lcd cam display", 4096, &video_cam_fd0, 4, &lcd_cam_task_handle, 0);     /* 新建摄像头显示任务 */

    camera_running = true;  // 设置摄像头运行标志

    return ESP_OK;
}

/**
 * @brief       摄像头显示任务
 * @param       arg: 传入参数(未用到)
 * @retval      无
 */
static void lcd_cam_task(void *arg)
{
    int video_fd = *((int *)arg);       /* 任务参数 */

    int res = 0;
    struct v4l2_buffer buf;             /* 视频缓冲信息 */

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_format format = {0};    /* 数据流格式 */
    format.type = type;                 

    /* 获取LCD BUF(双buffer) */
    void *lcd_buffer[2];
    void *draw_buffer = NULL;
    size_t data_cache_line_size = 0;

    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(lcddev.lcd_panel_handle, 2, &lcd_buffer[0], &lcd_buffer[1]));

    draw_buffer = lcd_buffer[1];
    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));

    /*  CAM BUF设置两个,使用MMAP */
    void *camera_outbuf[2];
    ESP_ERROR_CHECK(camera_set_bufs(video_fd, 2, NULL));    /* 设置CAMERA的帧缓存数量 */
    ESP_ERROR_CHECK(camera_get_bufs(2, &camera_outbuf[0])); /* 获取CAMERA的帧缓冲 */

    /*---------------PPA的设置-SRM------------------*/
    ppa_client_handle_t ppa_srm_handle = NULL;
    ppa_client_config_t ppa_srm_config = {                  /* 设置客户端的属性 */
        .oper_type             = PPA_OPERATION_SRM,         /* PPA操作类型(已注册的PPA客户只能请求一种) */
        .max_pending_trans_num = 1,                         /* 客户端可以持有的最大PPA事务挂起数量 */
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &ppa_srm_handle)); /* 注册PPA客户端 */

    ESP_ERROR_CHECK(camera_stream_start(video_fd));         /* 开启视频流 */

#if FPS_FUNC_ON == 1                                        /* FPS_FUNC_ON该宏 打开帧率显示功能 */
    int fps_count = 0;
    int64_t start_time = esp_timer_get_time();
#endif

    if (ioctl(video_fd, VIDIOC_G_FMT, &format) != 0)        /* 获取设置支持的视频格式 */
    {
        ESP_LOGE(mipi_cam_tag, "get fmt failed");
    }

    while (camera_running)  // 检查摄像头运行标志
    {
        draw_buffer = lcd_buffer[0] == draw_buffer ? lcd_buffer[1] : lcd_buffer[0];     /* 双缓冲情况下,切换buffer */

#if FPS_FUNC_ON == 1                                    /* FPS_FUNC_ON该宏 打开帧率显示功能 */
        fps_count++;
        if (fps_count == 50) {
            int64_t end_time = esp_timer_get_time();
            ESP_LOGI(mipi_cam_tag, "fps: %f", 1000000.0 / ((end_time - start_time) / 50.0));
            start_time = end_time;
            fps_count = 0;
        }
#endif
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;       /* 与前面format.type要一致 */
        buf.memory = V4L2_MEMORY_MMAP;                  /* 内存使用mmap方式 */
        
        res = ioctl(video_fd, VIDIOC_DQBUF, &buf);      /* 将已经捕获好视频的内存拉出已捕获视频的队列 */
        if (res != 0)
        {
            ESP_LOGE(mipi_cam_tag, "failed to receive video frame");
        }

        if (mipidev.id == 0x9881d)                                /* 5寸720P ILI9881D屏幕参数 */
		{
			ppa_srm_oper_config_t oper_config = {
				.in.buffer          = camera_outbuf[buf.index],   /* 输入图片数据源 */
				.in.pic_w           = format.fmt.pix.width,       /* 输入图片的宽度 */  
				.in.pic_h           = format.fmt.pix.height,      /* 输入图片的高度 */
				.in.block_w         = 720,                        /* 输入块的宽度 */
				.in.block_h         = 800,                        /* 输入块的高度 */
				.in.block_offset_x  = 0,                          /* 输入块的X方向偏移 */
				.in.block_offset_y  = 0,                          /* 输入块的Y方向偏移 */
				.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,  /* 输入图片的色彩模式 */

				.out.buffer         = draw_buffer,                /* 输出块的数据源 */
				.out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),   /* 数据源大小 */
				.out.pic_w          = lcddev.width,               /* 输出块的宽度(由输入的宽度、缩放因子和旋转角度决定) */
				.out.pic_h          = lcddev.height,              /* 输出块的高度(由输入的宽度、缩放因子和旋转角度决定) */
				.out.block_offset_x = 0,                          /* 输出图片的X方向偏移 */
				.out.block_offset_y = 240,                        /* 输出图片的Y方向偏移 */
				.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,  /* 输出图片的色彩模式 */
				.rotation_angle     = PPA_SRM_ROTATION_ANGLE_0,   /* 输出图片的旋转角度 */
				.scale_x            = 1.0f,                       /* 输出图片的X轴缩放因子 */
				.scale_y            = 1.0f,                       /* 输出图片的Y轴缩放因子 */
				.mirror_x           = false,                      /* 输出图片的X轴镜像选择 */
				.mode               = PPA_TRANS_MODE_BLOCKING,    /* PPA操作执行方式 */
			};
			ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);
	   }
	   else if (mipidev.id == 0x7703)                             /* 5.5寸720P ST7703屏幕参数 */
	   {
			ppa_srm_oper_config_t oper_config = {
				.in.buffer          = camera_outbuf[buf.index],   /* 输入图片数据源 */
				.in.pic_w           = format.fmt.pix.width,       /* 输入图片的宽度 */  
				.in.pic_h           = format.fmt.pix.height,      /* 输入图片的高度 */
				.in.block_w         = 720,                        /* 输入块的宽度 */
				.in.block_h         = 800,                        /* 输入块的高度 */
				.in.block_offset_x  = 0,                          /* 输入块的X方向偏移 */
				.in.block_offset_y  = 0,                          /* 输入块的Y方向偏移 */
				.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,  /* 输入图片的色彩模式 */

				.out.buffer         = draw_buffer,                /* 输出块的数据源 */
				.out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),   /* 数据源大小 */
				.out.pic_w          = lcddev.width,               /* 输出块的宽度(由输入的宽度、缩放因子和旋转角度决定) */
				.out.pic_h          = lcddev.height,              /* 输出块的高度(由输入的宽度、缩放因子和旋转角度决定) */
				.out.block_offset_x = 0,                          /* 输出图片的X方向偏移 */
				.out.block_offset_y = 240,                          /* 输出图片的Y方向偏移 */
				.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,  /* 输出图片的色彩模式 */
				.rotation_angle     = PPA_SRM_ROTATION_ANGLE_0,   /* 输出图片的旋转角度 */
				.scale_x            = 1.0f,                       /* 输出图片的X轴缩放因子 */
				.scale_y            = 1.0f,                       /* 输出图片的Y轴缩放因子 */
				.mirror_x           = false,                      /* 输出图片的X轴镜像选择 */
				.mode               = PPA_TRANS_MODE_BLOCKING,    /* PPA操作执行方式 */
			};
			ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);
	   }
	   else if (mipidev.id == 0x79007)                            /* 7寸 1024*600 EK79007屏幕参数 */
	   {
			ppa_srm_oper_config_t oper_config = {
				.in.buffer          = camera_outbuf[buf.index],   /* 输入图片数据源 */
				.in.pic_w           = format.fmt.pix.width,       /* 输入图片的宽度 */  
				.in.pic_h           = format.fmt.pix.height,      /* 输入图片的高度 */
				.in.block_w         = 800,                        /* 输入块的宽度 */
				.in.block_h         = 570,                        /* 输入块的高度 */
				.in.block_offset_x  = 0,                          /* 输入块的X方向偏移 */
				.in.block_offset_y  = 100,                        /* 输入块的Y方向偏移 */
				.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,  /* 输入图片的色彩模式 */

				.out.buffer         = draw_buffer,                /* 输出块的数据源 */
				.out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),   /* 数据源大小 */
				.out.pic_w          = lcddev.width,               /* 输出块的宽度(由输入的宽度、缩放因子和旋转角度决定) */
				.out.pic_h          = lcddev.height,              /* 输出块的高度(由输入的宽度、缩放因子和旋转角度决定) */
				.out.block_offset_x = 112,                        /* 输出图片的X方向偏移 */
				.out.block_offset_y = 30,                         /* 输出图片的Y方向偏移 */
				.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,  /* 输出图片的色彩模式 */
				.rotation_angle     = PPA_SRM_ROTATION_ANGLE_0,   /* 输出图片的旋转角度 */
				.scale_x            = 1.0f,                       /* 输出图片的X轴缩放因子 */
				.scale_y            = 1.0f,                       /* 输出图片的Y轴缩放因子 */
				.mirror_x           = false,                      /* 输出图片的X轴镜像选择 */
				.mode               = PPA_TRANS_MODE_BLOCKING,    /* PPA操作执行方式 */
			};
			ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);
	   }
	   else if (mipidev.id == 0x9881c8)                           /* 8寸800P ILI9881C屏幕参数 */
	   {
			ppa_srm_oper_config_t oper_config = {
				.in.buffer          = camera_outbuf[buf.index],   /* 输入图片数据源 */
				.in.pic_w           = format.fmt.pix.width,       /* 输入图片的宽度 */  
				.in.pic_h           = format.fmt.pix.height,      /* 输入图片的高度 */
				.in.block_w         = 800,                        /* 输入块的宽度 */
				.in.block_h         = 800,                        /* 输入块的高度 */
				.in.block_offset_x  = 0,                          /* 输入块的X方向偏移 */
				.in.block_offset_y  = 0,                          /* 输入块的Y方向偏移 */
				.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,  /* 输入图片的色彩模式 */

				.out.buffer         = draw_buffer,                /* 输出块的数据源 */
				.out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),   /* 数据源大小 */
				.out.pic_w          = lcddev.width,               /* 输出块的宽度(由输入的宽度、缩放因子和旋转角度决定) */
				.out.pic_h          = lcddev.height,              /* 输出块的高度(由输入的宽度、缩放因子和旋转角度决定) */
				.out.block_offset_x = 0,                          /* 输出图片的X方向偏移 */
				.out.block_offset_y = 240,                        /* 输出图片的Y方向偏移 */
				.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,  /* 输出图片的色彩模式 */
				.rotation_angle     = PPA_SRM_ROTATION_ANGLE_0,   /* 输出图片的旋转角度 */
				.scale_x            = 1.0f,                       /* 输出图片的X轴缩放因子 */
				.scale_y            = 1.0f,                       /* 输出图片的Y轴缩放因子 */
				.mirror_x           = false,                      /* 输出图片的X轴镜像选择 */
				.mode               = PPA_TRANS_MODE_BLOCKING,    /* PPA操作执行方式 */
			};
			ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);
	   }
	   else if (mipidev.id == 0x9881c10)                          /* 10.1寸800P ILI9881C屏幕参数 */
	   {
			ppa_srm_oper_config_t oper_config = {
				.in.buffer          = camera_outbuf[buf.index],   /* 输入图片数据源 */
				.in.pic_w           = format.fmt.pix.width,       /* 输入图片的宽度 */  
				.in.pic_h           = format.fmt.pix.height,      /* 输入图片的高度 */
				.in.block_w         = 800,                        /* 输入块的宽度 */
				.in.block_h         = 800,                        /* 输入块的高度 */
				.in.block_offset_x  = 0,                          /* 输入块的X方向偏移 */
				.in.block_offset_y  = 0,                          /* 输入块的Y方向偏移 */
				.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,  /* 输入图片的色彩模式 */

				.out.buffer         = draw_buffer,                /* 输出块的数据源 */
				.out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),   /* 数据源大小 */
				.out.pic_w          = lcddev.width,               /* 输出块的宽度(由输入的宽度、缩放因子和旋转角度决定) */
				.out.pic_h          = lcddev.height,              /* 输出块的高度(由输入的宽度、缩放因子和旋转角度决定) */
				.out.block_offset_x = 0,                          /* 输出图片的X方向偏移 */
				.out.block_offset_y = 240,                        /* 输出图片的Y方向偏移 */
				.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,  /* 输出图片的色彩模式 */
				.rotation_angle     = PPA_SRM_ROTATION_ANGLE_0,   /* 输出图片的旋转角度 */
				.scale_x            = 1.0f,                       /* 输出图片的X轴缩放因子 */
				.scale_y            = 1.0f,                       /* 输出图片的Y轴缩放因子 */
				.mirror_x           = false,                      /* 输出图片的X轴镜像选择 */
				.mode               = PPA_TRANS_MODE_BLOCKING,    /* PPA操作执行方式 */
			};
			ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);
	   }
	   
        if (ioctl(video_fd, VIDIOC_QBUF, &buf) != 0)    /* 将空闲的内存加入可捕获视频的队列 */
        {
            ESP_LOGE(mipi_cam_tag, "failed to free video frame");
        }

    }

	// 清理资源
    camera_stream_stop(video_fd);
    ppa_unregister_client(ppa_srm_handle);
    app_video_close(video_fd);
    
    ESP_LOGI(mipi_cam_tag, "Camera task stopped");
    vTaskDelete(NULL);

}

/**
 * @brief 停止摄像头任务
 * @param 无
 * @retval 无
 */
void mipi_cam_stop(void)
{
    camera_running = false;  // 设置停止标志
    
    if (lcd_cam_task_handle != NULL) {
        // 等待任务结束
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 如果任务还在运行，强制删除
        if (eTaskGetState(lcd_cam_task_handle) != eDeleted) {
            vTaskDelete(lcd_cam_task_handle);
        }
        lcd_cam_task_handle = NULL;
    }
}