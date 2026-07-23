#include "camera_display.h"
#include "app_video.h"
#include "board.h"
#include "display/lcd_display.h"

#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/i2c_master.h>
#include <esp_lvgl_port.h>
#include <esp_task_wdt.h>
#include <driver/ppa.h>
#include <esp_heap_caps.h>
#include <esp_cache.h>

#define ALIGN_UP_BY(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

#define CAM_RESET_PIN           GPIO_NUM_NC
#define CAM_PWDN_PIN            GPIO_NUM_NC

#define TAG "CameraDisplay"

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static lv_obj_t *s_canvas = NULL;
static lv_obj_t *s_loading_label = NULL;
static uint8_t *s_canvas_buf[3] = {NULL, NULL, NULL};
static uint8_t *s_display_buf = NULL;  // LVGL reads from this, PPA never touches it
static int s_frame_count = 0;
static int s_cam_width = 0;
static int s_cam_height = 0;
static int s_scaled_width = 0;
static int s_scaled_height = 0;
static size_t s_scaled_buf_size = 0;
static bool s_streaming = false;
static bool s_video_inited = false;
// ESP32-P4 L2 data cache line = 64 bytes
#define CAM_BUF_CACHE_ALIGN 64

static SemaphoreHandle_t s_web_mutex = NULL;
static uint8_t *s_web_frame = NULL;
static int s_web_width = 0;
static int s_web_height = 0;
static volatile bool s_web_frame_ready = false;

static bool s_thermal_effect = false;

int s_detection_x = 0;
int s_detection_y = 0;
int s_detection_w = 0;
int s_detection_h = 0;
bool s_detection_valid = false;
int64_t s_last_detection_us = 0;
static int64_t s_boot_lock_time_us = 0;
static uint16_t s_thermal_lut[256];
static bool s_thermal_lut_init = false;

static void init_thermal_lut(void)
{
    if (s_thermal_lut_init) return;
    for (int i = 0; i < 256; i++) {
        uint8_t r, g, b;
        if (i < 64) {
            r = 0;       g = 0;           b = i * 4;
        } else if (i < 128) {
            r = 0;       g = (i - 64) * 4; b = 255;
        } else if (i < 192) {
            r = (i - 128) * 4; g = 255;    b = 255 - (i - 128) * 4;
        } else {
            r = 255;     g = 255 - (i - 192) * 4; b = 0;
        }
        s_thermal_lut[i] = ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3);
    }
    s_thermal_lut_init = true;
}

static void apply_thermal_effect(uint8_t *buf, int w, int h)
{
    init_thermal_lut();
    uint16_t *pixels = (uint16_t *)buf;
    int count = w * h;
    for (int i = 0; i < count; i++) {
        uint16_t p = pixels[i];
        uint8_t r = ((p >> 11) & 0x1F) << 3;
        uint8_t g = ((p >> 5) & 0x3F) << 2;
        uint8_t b = (p & 0x1F) << 3;
        uint8_t gray = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
        pixels[i] = s_thermal_lut[gray];
    }
}

void camera_display_set_thermal_effect(bool on)
{
    s_thermal_effect = on;
}

bool camera_display_get_thermal_effect(void)
{
    return s_thermal_effect;
}

void camera_display_set_i2c_bus(i2c_master_bus_handle_t bus)
{
    s_i2c_bus = bus;
}

lv_obj_t* camera_display_create(lv_obj_t *parent)
{
    lv_coord_t sw = lv_disp_get_hor_res(NULL);
    lv_coord_t sh = lv_disp_get_ver_res(NULL);

    lv_obj_t *bg = lv_obj_create(parent);
    lv_obj_set_size(bg, sw, sh);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bg, 0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

    s_loading_label = lv_label_create(bg);
    lv_label_set_text(s_loading_label, "Camera starting...");
    lv_obj_center(s_loading_label);
    lv_obj_set_style_text_color(s_loading_label, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);

    return bg;
}

static void camera_task(void *arg)
{
    if (!s_video_inited) {
        esp_err_t err = app_video_main(s_i2c_bus);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init video subsystem: %s", esp_err_to_name(err));
            if (s_loading_label) {
                lvgl_port_lock(0);
                lv_label_set_text(s_loading_label, "Video init fail");
                lvgl_port_unlock();
            }
            vTaskDelete(NULL);
            return;
        }
        s_video_inited = true;
    }

    int fd = app_video_open(0);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open video device");
        if (s_loading_label) {
            lvgl_port_lock(0);
            lv_label_set_text(s_loading_label, "Camera not found");
            lvgl_port_unlock();
        }
        vTaskDelete(NULL);
        return;
    }

    if (app_video_init(fd, APP_VIDEO_FMT_RGB565) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init camera format");
        if (s_loading_label) {
            lvgl_port_lock(0);
            lv_label_set_text(s_loading_label, "Format fail");
            lvgl_port_unlock();
        }
        vTaskDelete(NULL);
        return;
    }

    struct v4l2_format fmt;
    std::memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) != 0) {
        ESP_LOGE(TAG, "Failed to get format");
        close(fd);
        vTaskDelete(NULL);
        return;
    }

    s_cam_width = fmt.fmt.pix.width;
    s_cam_height = fmt.fmt.pix.height;
    ESP_LOGI(TAG, "Camera resolution: %dx%d", s_cam_width, s_cam_height);

    lv_coord_t sw = lv_disp_get_hor_res(NULL);
    lv_coord_t sh = lv_disp_get_ver_res(NULL);
    double display_scale = 1.0;
    if (s_cam_width > sw || s_cam_height > sh) {
        display_scale = LV_MIN((double)sw / s_cam_width, (double)sh / s_cam_height);
    }
    s_scaled_width = (int)(s_cam_width * display_scale);
    s_scaled_height = (int)(s_cam_height * display_scale);

    s_scaled_buf_size = (size_t)s_scaled_width * s_scaled_height * 2;
    s_scaled_buf_size = ALIGN_UP_BY(s_scaled_buf_size, CAM_BUF_CACHE_ALIGN);

    for (int i = 0; i < 3; i++) {
        s_canvas_buf[i] = (uint8_t*)heap_caps_aligned_alloc(CAM_BUF_CACHE_ALIGN, s_scaled_buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        if (!s_canvas_buf[i]) {
            ESP_LOGE(TAG, "Failed to allocate canvas buffer %d (%zu bytes)", i, s_scaled_buf_size);
            for (int j = 0; j < i; j++) heap_caps_free(s_canvas_buf[j]);
            close(fd);
            vTaskDelete(NULL);
            return;
        }
    }
    s_frame_count = 0;
    ESP_LOGI(TAG, "Allocated 3x PPA output buffers %zu bytes each", s_scaled_buf_size);

    // Allocate web streaming frame buffer
    if (s_web_mutex && !s_web_frame) {
        s_web_frame = (uint8_t*)heap_caps_aligned_alloc(CAM_BUF_CACHE_ALIGN, s_scaled_buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        if (!s_web_frame) {
            ESP_LOGW(TAG, "Failed to allocate web frame buffer");
        }
    }

    // Register PPA SRM client
    ppa_client_handle_t ppa_srm = NULL;
    ppa_client_config_t ppa_cfg = {
        .oper_type = PPA_OPERATION_SRM,
        .max_pending_trans_num = 1,
        .data_burst_length = PPA_DATA_BURST_LENGTH_128,
    };
    esp_err_t ppa_ret = ppa_register_client(&ppa_cfg, &ppa_srm);
    if (ppa_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register PPA client: %s", esp_err_to_name(ppa_ret));
        for (int i = 0; i < 3; i++) {
            if (s_canvas_buf[i]) heap_caps_free(s_canvas_buf[i]);
            s_canvas_buf[i] = NULL;
        }
        close(fd);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "PPA SRM client registered");

    if (!lvgl_port_lock(1000)) {
        ESP_LOGE(TAG, "Failed to lock LVGL for camera canvas init");
        ppa_unregister_client(ppa_srm);
        for (int i = 0; i < 3; i++) {
            if (s_canvas_buf[i]) heap_caps_free(s_canvas_buf[i]);
            s_canvas_buf[i] = NULL;
        }
        close(fd);
        vTaskDelete(NULL);
        return;
    }

    if (s_loading_label) {
        lv_obj_t *parent = lv_obj_get_parent(s_loading_label);
        lv_obj_del(s_loading_label);
        s_loading_label = NULL;

        s_canvas = lv_canvas_create(parent);
        lv_canvas_set_buffer(s_canvas, s_canvas_buf[0], s_scaled_width, s_scaled_height, LV_IMG_CF_TRUE_COLOR);
        lv_obj_center(s_canvas);
        // LVGL software scaling to fit screen
    }

    lvgl_port_unlock();

    if (camera_set_bufs(fd, 3, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set up buffers");
        ppa_unregister_client(ppa_srm);
        vTaskDelete(NULL);
        return;
    }

    void *fb[3];
    if (camera_get_bufs(3, fb) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get buffers");
        close(fd);
        ppa_unregister_client(ppa_srm);
        vTaskDelete(NULL);
        return;
    }

    if (camera_stream_start(fd) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start stream");
        close(fd);
        ppa_unregister_client(ppa_srm);
        vTaskDelete(NULL);
        return;
    }

    s_streaming = true;
    ESP_LOGI(TAG, "Camera streaming started %dx%d -> %dx%d via PPA",
             s_cam_width, s_cam_height, s_scaled_width, s_scaled_height);

    // 开机锁屏: 显示黑色锁屏界面, 检测到人脸后自动解锁
    {
        auto* display = Board::GetInstance().GetDisplay();
        auto* lcd = static_cast<LcdDisplay*>(display);
        lvgl_port_lock(0);
        lcd->ShowLockScreen();
        lvgl_port_unlock();
        s_boot_lock_time_us = esp_timer_get_time();
        ESP_LOGI(TAG, "Boot lock screen shown, 3s guard active");
    }

    esp_task_wdt_add(NULL);
    while (s_streaming) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buf) != 0) {
            ESP_LOGE(TAG, "Failed to dequeue buffer");
            break;
        }

        int write_idx = s_frame_count % 3;
        s_frame_count++;

        // Invalidate cache for camera V4L2 buffer so PPA DMA reads fresh data, not stale L2 cache
        size_t cam_buf_bytes = (size_t)s_cam_width * s_cam_height * 2;
        esp_cache_msync(fb[buf.index], ALIGN_UP_BY(cam_buf_bytes, CAM_BUF_CACHE_ALIGN),
                        ESP_CACHE_MSYNC_FLAG_INVALIDATE | ESP_CACHE_MSYNC_FLAG_UNALIGNED);

        // PPA hardware scaling to fit display
        ppa_srm_oper_config_t oper = {
            .in = {
                .buffer = fb[buf.index],
                .pic_w = (uint32_t)s_cam_width,
                .pic_h = (uint32_t)s_cam_height,
                .block_w = (uint32_t)s_cam_width,
                .block_h = (uint32_t)s_cam_height,
                .block_offset_x = 0,
                .block_offset_y = 0,
                .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
            },
            .out = {
                .buffer = s_canvas_buf[write_idx],
                .buffer_size = (uint32_t)s_scaled_buf_size,
                .pic_w = (uint32_t)s_scaled_width,
                .pic_h = (uint32_t)s_scaled_height,
                .block_offset_x = 0,
                .block_offset_y = 0,
                .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
            },
            .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
            .scale_x = (float)display_scale,
            .scale_y = (float)display_scale,
            .mirror_x = false,
            .mirror_y = false,
            .rgb_swap = false,
            .byte_swap = false,
            .alpha_update_mode = PPA_ALPHA_NO_CHANGE,
            .mode = PPA_TRANS_MODE_BLOCKING,
        };
        ppa_ret = ppa_do_scale_rotate_mirror(ppa_srm, &oper);
        if (ppa_ret != ESP_OK) {
            ESP_LOGE(TAG, "PPA scale failed: %s", esp_err_to_name(ppa_ret));
        }

        // Invalidate cache so CPU/LVGL reads actual PPA-written data, not stale cache
        esp_cache_msync(s_canvas_buf[write_idx], s_scaled_buf_size,
                        ESP_CACHE_MSYNC_FLAG_INVALIDATE | ESP_CACHE_MSYNC_FLAG_UNALIGNED);

        if (s_thermal_effect) {
            apply_thermal_effect(s_canvas_buf[write_idx], s_scaled_width, s_scaled_height);
            esp_cache_msync(s_canvas_buf[write_idx], (size_t)s_scaled_width * s_scaled_height * 2,
                            ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        }

        // 绿框: 跟随人体质心, 5 秒无检测则消失
        bool show_box = s_detection_valid ||
                        (s_last_detection_us > 0 &&
                         esp_timer_get_time() - s_last_detection_us < 5000000);

        // 首次检测到人脸 → 解锁 (开机3秒保护, 防止刚启动时误解锁)
        static bool face_unlocked = false;
        if (!face_unlocked && show_box &&
            s_boot_lock_time_us > 0 &&
            esp_timer_get_time() - s_boot_lock_time_us >= 3000000) {
            face_unlocked = true;
            ESP_LOGI(TAG, "Face detected → unlock");
            auto* display = Board::GetInstance().GetDisplay();
            auto* lcd = static_cast<LcdDisplay*>(display);
            lvgl_port_lock(0);
            lcd->HideLockScreen();
            lvgl_port_unlock();
        }

        if (show_box) {
            uint16_t green = 0x07E0;
            int x1 = s_detection_x, y1 = s_detection_y;
            int x2 = x1 + s_detection_w - 1, y2 = y1 + s_detection_h - 1;
            x1 = x1 < 0 ? 0 : x1; y1 = y1 < 0 ? 0 : y1;
            x2 = x2 >= s_scaled_width ? s_scaled_width - 1 : x2;
            y2 = y2 >= s_scaled_height ? s_scaled_height - 1 : y2;
            uint16_t* buf16 = (uint16_t*)s_canvas_buf[write_idx];

            auto draw_pixel = [&](int px, int py) {
                if (px >= 0 && px < s_scaled_width && py >= 0 && py < s_scaled_height)
                    buf16[py * s_scaled_width + px] = green;
            };

            for (int x = x1; x <= x2; x++) {
                draw_pixel(x, y1); draw_pixel(x, y1 + 1);
                draw_pixel(x, y2); draw_pixel(x, y2 - 1);
            }
            for (int y = y1; y <= y2; y++) {
                draw_pixel(x1, y); draw_pixel(x1 + 1, y);
                draw_pixel(x2, y); draw_pixel(x2 - 1, y);
            }

            esp_cache_msync(s_canvas_buf[write_idx], (size_t)s_scaled_width * s_scaled_height * 2,
                            ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        }

        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "Failed to queue buffer");
            break;
        }

        // Copy frame for web streaming (already scaled)
        if (s_web_mutex && s_web_frame) {
            if (xSemaphoreTake(s_web_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                memcpy(s_web_frame, s_canvas_buf[write_idx], s_scaled_width * s_scaled_height * 2);
                s_web_width = s_scaled_width;
                s_web_height = s_scaled_height;
                s_web_frame_ready = true;
                xSemaphoreGive(s_web_mutex);
            }
        }

        // 拷贝到专用显示缓冲区，消除 PPA 写入与 LVGL 读取的竞态
        if (!s_display_buf) {
            s_display_buf = (uint8_t*)heap_caps_aligned_alloc(CAM_BUF_CACHE_ALIGN, s_scaled_buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        if (s_display_buf) {
            memcpy(s_display_buf, s_canvas_buf[write_idx], s_scaled_width * s_scaled_height * 2);
            // Invalidate display cache so LVGL reads fresh memcpy'd data
            esp_cache_msync(s_display_buf, s_scaled_buf_size,
                            ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        }

        if (s_canvas) {
            if (lvgl_port_lock(100)) {
                lv_canvas_set_buffer(s_canvas, s_display_buf,
                                     s_scaled_width, s_scaled_height, LV_IMG_CF_TRUE_COLOR);
                lv_obj_invalidate(s_canvas);
                lvgl_port_unlock();
            }
        }

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
    esp_task_wdt_delete(NULL);

    camera_stream_stop(fd);
    close(fd);
    ppa_unregister_client(ppa_srm);
    s_streaming = false;
    vTaskDelete(NULL);
}

esp_err_t camera_get_frame(uint8_t *out_buf, size_t buf_size, int *width, int *height)
{
    if (!s_web_mutex || !s_web_frame || !s_web_frame_ready) {
        return ESP_FAIL;
    }
    if (xSemaphoreTake(s_web_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *width = s_web_width;
        *height = s_web_height;
        if (out_buf != NULL) {
            size_t needed = (size_t)s_web_width * s_web_height * 2;
            if (buf_size < needed) {
                xSemaphoreGive(s_web_mutex);
                return ESP_ERR_INVALID_SIZE;
            }
            memcpy(out_buf, s_web_frame, needed);
        }
        xSemaphoreGive(s_web_mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

void camera_display_start(void)
{
    s_web_mutex = xSemaphoreCreateMutex();
    if (!s_web_mutex) {
        ESP_LOGE(TAG, "Failed to create web frame mutex");
    }
    xTaskCreate(camera_task, "camera_disp", 16384, NULL, 2, NULL);
}
