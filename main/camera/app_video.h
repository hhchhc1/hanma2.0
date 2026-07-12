#ifndef __APP_VIDEO_H
#define __APP_VIDEO_H

#include "esp_err.h"
#include "linux/videodev2.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_VIDEO_FMT_RAW8 = V4L2_PIX_FMT_SBGGR8,
    APP_VIDEO_FMT_RAW10 = V4L2_PIX_FMT_SBGGR10,
    APP_VIDEO_FMT_GREY = V4L2_PIX_FMT_GREY,
    APP_VIDEO_FMT_RGB565 = V4L2_PIX_FMT_RGB565,
    APP_VIDEO_FMT_RGB888 = V4L2_PIX_FMT_RGB24,
    APP_VIDEO_FMT_YUV422 = V4L2_PIX_FMT_YUV422P,
    APP_VIDEO_FMT_YUV420 = V4L2_PIX_FMT_YUV420,
} video_fmt_t;

esp_err_t app_video_init(int fd, video_fmt_t init_fmt);
esp_err_t app_video_main(i2c_master_bus_handle_t i2c_bus_handle);
int app_video_open(int port);
esp_err_t camera_stream_start(int video_fd);
esp_err_t camera_stream_stop(int video_fd);
esp_err_t camera_set_bufs(int video_fd, int fb_num, void **fb);
esp_err_t camera_get_bufs(int fb_num, void **fb);
uint32_t camera_get_buf_size(int fb_num);

#ifdef __cplusplus
}
#endif

#endif
