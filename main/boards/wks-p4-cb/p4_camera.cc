#include "p4_camera.h"
#include "camera/camera_display.h"
#include "board.h"
#include "application.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include <driver/gpio.h>
#include <cstring>
#include <cmath>
#include <vector>

#define BULB_GPIO GPIO_NUM_12

#define TAG "P4Camera"

#define SKIN_R_MIN 50
#define SKIN_G_MIN 20
#define SKIN_DIFF_RG 5
#define FACE_MIN_AREA 30
#define FACE_MAX_AREA 20000
#define FACE_DILATE 3

static inline void rgb565_to_rgb(uint16_t p, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = ((p >> 11) & 0x1F) << 3;
    g = ((p >> 5) & 0x3F) << 2;
    b = (p & 0x1F) << 3;
}
static inline bool is_skin(uint8_t r, uint8_t g, uint8_t b) {
    return r > SKIN_R_MIN && g > SKIN_G_MIN && r > g && g > b && (r - g) > SKIN_DIFF_RG;
}
static inline uint8_t rgb565_to_gray(uint16_t p) {
    uint8_t r, g, b; rgb565_to_rgb(p, r, g, b);
    return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
}

P4Camera::P4Camera() {
    ESP_LOGI(TAG, "P4Camera created");
    StartMotionDetection(300);
}

P4Camera::~P4Camera() {
    if (frame_buf_) heap_caps_free(frame_buf_);
    if (gray_buf_) heap_caps_free(gray_buf_);
    if (prev_gray_) heap_caps_free(prev_gray_);
    if (skin_mask_) heap_caps_free(skin_mask_);
    if (labels_) heap_caps_free(labels_);
}

bool P4Camera::Capture() {
    int w = 0, h = 0;
    if (camera_get_frame(NULL, 0, &w, &h) != ESP_OK || w <= 0 || h <= 0) {
        ESP_LOGE(TAG, "Camera frame not ready");
        return false;
    }

    size_t frame_size = (size_t)w * h * 2;
    uint8_t* buf = (uint8_t*)heap_caps_malloc(frame_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer");
        return false;
    }

    if (camera_get_frame(buf, frame_size, &w, &h) != ESP_OK) {
        heap_caps_free(buf);
        ESP_LOGE(TAG, "Failed to get camera frame");
        return false;
    }

    if (frame_buf_) heap_caps_free(frame_buf_);
    frame_buf_ = buf;
    frame_size_ = frame_size;
    width_ = w;
    height_ = h;

    int pixel_count = w * h;
    if (!gray_buf_) gray_buf_ = (uint8_t*)heap_caps_malloc(pixel_count, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!prev_gray_) prev_gray_ = (uint8_t*)heap_caps_malloc(pixel_count, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    // 预分配肤色+连通域缓存 (持久化避免每帧 malloc/free)
    if (!buffers_ready_) {
        int ftotal = w * h;
        skin_mask_ = (uint8_t*)heap_caps_malloc(ftotal, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        labels_ = (int*)heap_caps_malloc(ftotal * sizeof(int), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (skin_mask_ && labels_) buffers_ready_ = true;
    }

    const uint16_t* src = (const uint16_t*)frame_buf_;
    for (int i = 0; i < pixel_count; i++) {
        gray_buf_[i] = rgb565_to_gray(src[i]);
    }

    return true;
}

bool P4Camera::DetectMotion(const uint8_t* gray, int w, int h) {
    if (!prev_gray_ || !gray || w <= 0 || h <= 0) return false;

    int total = w * h;
    static bool first_frame = true;
    if (first_frame) {
        memcpy(prev_gray_, gray, total);
        first_frame = false;
        return false;
    }

    // === 肤色检测 (质心 + 标准差, 自适应人脸大小) ===
    // 仅在有运动时启动, 避免背景杂物触发绿框
    static int smooth_cx = -1, smooth_cy = -1;
    static int64_t last_motion_us = 0;

    if (frame_buf_) {
        const uint16_t* rgb = (const uint16_t*)frame_buf_;
        int fw = width_, fh = height_;
        int scan_h = (int)(fh * 0.40f);
        if (scan_h < 1) scan_h = 1;

        int64_t sx = 0, sy = 0, sxx = 0, syy = 0;
        int skin_pts = 0;

        for (int y = 0; y < scan_h; y++) {
            for (int x = 0; x < fw; x++) {
                uint16_t p = rgb[y * fw + x];
                uint8_t r, g, b; rgb565_to_rgb(p, r, g, b);
                if (!is_skin(r, g, b)) continue;
                sx += x; sy += y; sxx += x * x; syy += y * y;
                skin_pts++;
            }
        }

        bool recent_motion = (esp_timer_get_time() - last_motion_us) < 3000000;

        if (recent_motion && skin_pts >= 30) {
            int cx = (int)(sx / skin_pts);
            int cy = (int)(sy / skin_pts);

            if (smooth_cx < 0) { smooth_cx = cx; smooth_cy = cy; }
            else { smooth_cx = (int)(smooth_cx * 0.4f + cx * 0.6f);
                   smooth_cy = (int)(smooth_cy * 0.4f + cy * 0.6f); }

            int bw, bh;
            if (skin_pts < 200) {
                bw = 80; bh = 100;
            } else {
                float vx = (float)(sxx - (int64_t)cx * cx) / skin_pts;
                float vy = (float)(syy - (int64_t)cy * cy) / skin_pts;
                bw = (int)(sqrtf(vx >= 0 ? vx : 0) * 2.0f);
                bh = (int)(sqrtf(vy >= 0 ? vy : 0) * 2.0f);
                if (bw < 60) { bw = 60; }
                if (bw > 220) { bw = 220; }
                if (bh < 80) { bh = 80; }
                if (bh > 320) { bh = 320; }
            }

            s_detection_x = smooth_cx - bw / 2;
            s_detection_y = smooth_cy - bh / 2;
            s_detection_w = bw;
            s_detection_h = bh;
            s_detection_valid = true;
            s_last_detection_us = esp_timer_get_time();
        } else {
            s_detection_valid = false;
            s_last_detection_us = 0;
            smooth_cx = -1;
        }
    }

    // === 帧差法 (运动检测, 用于 bulb/sound 触发 + 绿框启动门控) ===
    uint8_t* binary = (uint8_t*)heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!binary) return false;
    int changed = 0;
    for (int i = 0; i < total; i++) {
        binary[i] = (abs((int)gray[i] - (int)prev_gray_[i]) > 45) ? 1 : 0;
        changed += binary[i];
    }
    memcpy(prev_gray_, gray, total);

    if ((float)changed / (float)total >= 0.05f) {
        last_motion_us = esp_timer_get_time();
        ESP_LOGD(TAG, "Motion: %d pixels", changed);
        heap_caps_free(binary);
        return true;
    }
    heap_caps_free(binary);
    return false;
}

uint8_t* P4Camera::GetGrayFrame() {
    return gray_buf_;
}

void P4Camera::StartMotionDetection(int interval_ms) {
    motion_interval_ms_ = interval_ms;
    if (motion_task_) return;
    xTaskCreatePinnedToCore(MotionTaskWrapper, "motion_detect", 4096, this, 1, &motion_task_, 0);
    ESP_LOGI(TAG, "Motion detection started, interval=%dms", interval_ms);
}

void P4Camera::StopMotionDetection() {
    if (motion_task_) {
        vTaskDelete(motion_task_);
        motion_task_ = nullptr;
        ESP_LOGI(TAG, "Motion detection stopped");
    }
}

void P4Camera::MotionTaskWrapper(void* arg) {
    static_cast<P4Camera*>(arg)->MotionTaskLoop();
}

void P4Camera::MotionTaskLoop() {
    gpio_set_direction(BULB_GPIO, GPIO_MODE_OUTPUT);

    gpio_set_level(BULB_GPIO, 1);
    ESP_LOGI(TAG, "Motion task started, bulb test ON");
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(BULB_GPIO, 0);
    ESP_LOGI(TAG, "Bulb test OFF");

    while (true) {
        if (Capture()) {
            if (DetectMotion(gray_buf_, width_, height_)) {
                ESP_LOGI(TAG, "Motion! Toggle bulb 500ms");
                gpio_set_level(BULB_GPIO, 1);

                vTaskDelay(pdMS_TO_TICKS(500));
                gpio_set_level(BULB_GPIO, 0);
                if (motion_cb_) motion_cb_();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(motion_interval_ms_));
    }
}

bool P4Camera::SetHMirror(bool enabled) {
    ESP_LOGW(TAG, "SetHMirror not implemented for V4L2 camera");
    return false;
}

bool P4Camera::SetVFlip(bool enabled) {
    ESP_LOGW(TAG, "SetVFlip not implemented for V4L2 camera");
    return false;
}
