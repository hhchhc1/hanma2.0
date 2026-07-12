#ifndef P4_CAMERA_H
#define P4_CAMERA_H

#include "camera.h"
#include <string>

#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class P4Camera : public Camera {
public:
    using MotionCallback = std::function<void()>;

    P4Camera();
    ~P4Camera();

    bool Capture() override;
    bool SetHMirror(bool enabled) override;
    bool SetVFlip(bool enabled) override;

    bool DetectMotion(const uint8_t* gray, int w, int h);
    uint8_t* GetGrayFrame();

    void OnMotion(MotionCallback cb) { motion_cb_ = std::move(cb); }
    void StartMotionDetection(int interval_ms = 300);
    void StopMotionDetection();

private:
    static void MotionTaskWrapper(void* arg);
    void MotionTaskLoop();

    uint8_t* frame_buf_ = nullptr;
    size_t frame_size_ = 0;
    uint8_t* gray_buf_ = nullptr;
    uint8_t* prev_gray_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    uint8_t* skin_mask_ = nullptr;
    int* labels_ = nullptr;
    bool buffers_ready_ = false;

    MotionCallback motion_cb_;
    TaskHandle_t motion_task_ = nullptr;
    int motion_interval_ms_ = 300;
};

#endif
