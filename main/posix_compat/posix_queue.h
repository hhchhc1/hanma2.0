/**
 * @file posix_queue.h
 * @brief 线程安全消息队列 — POSIX 风格封装，底层 FreeRTOS
 *
 * 底层使用 FreeRTOS 队列保证 ISR/回调线程安全（不依赖 pthread）。
 * 对外提供标准 C++ 模板接口。
 *
 * 参考 OpenVela (NuttX) 的 POSIX mq_* 接口风格。
 */
#pragma once

#include <cstddef>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#ifdef __cplusplus
}
#endif

namespace posix {

template <typename T>
class MessageQueue {
public:
    explicit MessageQueue(size_t max_size = 32)
        : handle_(xQueueCreate(max_size, sizeof(T))) {}

    ~MessageQueue() {
        if (handle_) {
            vQueueDelete(handle_);
        }
    }

    MessageQueue(const MessageQueue &) = delete;
    MessageQueue &operator=(const MessageQueue &) = delete;

    /** 阻塞发送 (timeout_ms=0 → 无限等待) */
    bool send(const T &item, uint32_t timeout_ms = 0) {
        TickType_t ticks = (timeout_ms == 0)
            ? portMAX_DELAY
            : pdMS_TO_TICKS(timeout_ms);
        return xQueueSend(handle_, &item, ticks) == pdTRUE;
    }

    /** 非阻塞发送 */
    bool send_nonblocking(const T &item) {
        return xQueueSend(handle_, &item, 0) == pdTRUE;
    }

    /** 阻塞接收 (timeout_ms=0 → 无限等待) */
    bool receive(T &item, uint32_t timeout_ms = 0) {
        TickType_t ticks = (timeout_ms == 0)
            ? portMAX_DELAY
            : pdMS_TO_TICKS(timeout_ms);
        return xQueueReceive(handle_, &item, ticks) == pdTRUE;
    }

    /** 非阻塞接收 */
    bool receive_nonblocking(T &item) {
        return xQueueReceive(handle_, &item, 0) == pdTRUE;
    }

    bool empty() const     { return uxQueueMessagesWaiting(handle_) == 0; }
    size_t size() const    { return uxQueueMessagesWaiting(handle_); }

private:
    QueueHandle_t handle_;
};

} // namespace posix
