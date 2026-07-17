/**
 * @file posix_timer.h
 * @brief POSIX 定时器封装 — 替代 esp_timer + vTaskDelay 定时模式
 *
 * 提供周期性定时器创建/启动/停止，基于 std::thread + sleep_until 实现。
 * 对于需要 ISR 级精度的场景（如音频采样时钟），底层仍保留 esp_timer。
 * 此封装适用于应用层周期性任务（传感器轮询、UI 刷新等）。
 *
 * 参考 OpenVela (NuttX) timer_create 接口风格。
 */
#pragma once

#include <thread>
#include <atomic>
#include <functional>
#include <chrono>

namespace posix {

class Timer {
public:
    using Callback = std::function<void()>;

    /**
     * @brief 创建定时器
     * @param name       名称 (日志用)
     * @param cb         回调函数
     * @param period_ms  周期 (毫秒)
     * @param oneshot    true=单次, false=重复
     */
    Timer(const char *name, Callback cb, uint32_t period_ms, bool oneshot = false)
        : name_(name), cb_(std::move(cb)), period_ms_(period_ms),
          oneshot_(oneshot), running_(false) {}

    ~Timer() { stop(); }

    /**
     * @brief 启动定时器
     */
    void start() {
        if (running_.exchange(true)) return;  // 已在运行

        printf("[POSIX_TIMER] Starting '%s' period=%lums %s\n",
               name_, (unsigned long)period_ms_, oneshot_ ? "oneshot" : "repeating");

        thread_ = std::thread([this]() {
            auto next = std::chrono::steady_clock::now();

            do {
                next += std::chrono::milliseconds(period_ms_);

                if (cb_) cb_();

                // 精确睡眠到下一个周期时刻
                std::this_thread::sleep_until(next);

            } while (!oneshot_ && running_.load(std::memory_order_relaxed));
        });
    }

    /**
     * @brief 停止定时器
     */
    void stop() {
        if (!running_.exchange(false)) return;  // 已停止

        printf("[POSIX_TIMER] Stopping '%s'\n", name_);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    /**
     * @brief 是否正在运行
     */
    bool is_running() const {
        return running_.load(std::memory_order_relaxed);
    }

private:
    const char *name_;
    Callback cb_;
    uint32_t period_ms_;
    bool oneshot_;
    std::atomic<bool> running_;
    std::thread thread_;
};

} // namespace posix

/*
 * 兼容宏映射:
 *
 *   原 esp_timer API                                 → POSIX 封装
 *   ─────────────────────────────────────────────────────────────────
 *   esp_timer_create(&args, &handle)                 → auto* t = new posix::Timer(name, cb, ms);
 *   esp_timer_start_periodic(handle, period_us)       → t->start()
 *   esp_timer_start_once(handle, timeout_us)          → 将 oneshot=true
 *   esp_timer_stop(handle)                           → t->stop()
 *   esp_timer_delete(handle)                         → delete t
 *
 * vTaskDelay 便捷宏:
 *   vTaskDelay(pdMS_TO_TICKS(ms))  →  std::this_thread::sleep_for(std::chrono::milliseconds(ms))
 *   vTaskDelay(pdMS_TO_TICKS(10))  →  std::this_thread::sleep_for(std::chrono::milliseconds(10))
 */

// 便捷延迟宏
#define POSIX_DELAY_MS(ms) \
    std::this_thread::sleep_for(std::chrono::milliseconds(ms))


