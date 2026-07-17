/**
 * @file posix_event.h
 * @brief POSIX 事件组 — 替代 FreeRTOS xEventGroup
 *
 * 底层使用 FreeRTOS 原生 EventGroup（保证 ISR/回调线程安全），
 * 对外提供 pthread 风格的标准 C++ 接口。
 *
 * 为什么底层不能直接用 pthread_mutex？
 *   ESP-IDF 的 pthread 要求线程必须由 pthread_create() 创建。
 *   MQTT/WiFi/ESP-IDF 内部的回调运行在 FreeRTOS 原生任务上，
 *   这些任务没有 pthread 注册，调用 pthread_self() 会断言失败。
 *   因此底层必须使用 FreeRTOS API，仅在调用方是 pthread 的模块
 *   （如 main_event_loop、audio 管线任务）中才走 posix::task_create。
 *
 * 参考 OpenVela (NuttX) 的事件通知机制 — API 层标准化，底层按平台最优实现。
 */
#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#ifdef __cplusplus
}
#endif

namespace posix {

class EventGroup {
public:
    EventGroup() : handle_(xEventGroupCreate()) {}

    ~EventGroup() {
        if (handle_) {
            vEventGroupDelete(handle_);
            handle_ = nullptr;
        }
    }

    // 禁止拷贝/移动
    EventGroup(const EventGroup &) = delete;
    EventGroup &operator=(const EventGroup &) = delete;

    /**
     * @brief 设置事件位（线程安全，可从任意线程调用）
     */
    uint32_t set_bits(uint32_t bits) {
        return xEventGroupSetBits(handle_, bits);
    }

    /**
     * @brief 清除事件位
     */
    uint32_t clear_bits(uint32_t bits) {
        return xEventGroupClearBits(handle_, bits);
    }

    /**
     * @brief 等待事件位
     *
     * @param bits           要等待的位掩码
     * @param clear_on_exit  true → pdTRUE (返回后自动清除)
     * @param wait_for_all   true → 等待所有位, false → 等待任意位
     * @param timeout_ms     超时毫秒, 0 → portMAX_DELAY (无限等待)
     * @return 等待返回时的事件组值
     */
    uint32_t wait_bits(uint32_t bits, bool clear_on_exit,
                       bool wait_for_all, uint32_t timeout_ms) {
        TickType_t ticks = (timeout_ms == 0)
            ? portMAX_DELAY
            : pdMS_TO_TICKS(timeout_ms);
        return xEventGroupWaitBits(handle_, bits,
            clear_on_exit ? pdTRUE : pdFALSE,
            wait_for_all  ? pdTRUE : pdFALSE,
            ticks);
    }

    /**
     * @brief 非阻塞读取当前事件位
     */
    uint32_t get_bits() const {
        return xEventGroupGetBits(handle_);
    }

    /** 获取底层句柄（供 ISR 回调使用 FromISR 系列 API） */
    EventGroupHandle_t handle() const { return handle_; }

private:
    EventGroupHandle_t handle_;
};

/**
 * @brief 便捷函数 — 创建事件组
 */
static inline EventGroup *event_group_create() {
    return new EventGroup();
}

/**
 * @brief 便捷函数 — 销毁事件组
 */
static inline void event_group_delete(EventGroup *eg) {
    delete eg;
}

} // namespace posix

/*
 * 接口映射:
 *
 *   EventGroupHandle_t eg = xEventGroupCreate(); → auto* eg = posix::event_group_create();
 *   xEventGroupSetBits(eg, BITS)                 → eg->set_bits(BITS)
 *   xEventGroupWaitBits(eg, B, clr, all, T/O)    → eg->wait_bits(B, clr, all, timeout_ms)
 *   xEventGroupClearBits(eg, BITS)               → eg->clear_bits(BITS)
 *   xEventGroupGetBits(eg)                       → eg->get_bits()
 *   vEventGroupDelete(eg)                        → posix::event_group_delete(eg)
 *
 * 超时值:
 *   portMAX_DELAY       → 0
 *   pdMS_TO_TICKS(ms)   → ms (直接传毫秒)
 */
