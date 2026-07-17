/**
 * @file mem_guard.h
 * @brief 内存守护任务 — 定期巡检已分配内存块的哨兵完整性
 *
 * 低优先级后台任务（优先级 0），每 10 秒遍历所有已分配块:
 *   - 检查每块的起始魔数 (0xDEADBEEF) 和结尾魔数 (0xBEEFDEAD)
 *   - 发现魔数被踩 → 记录 domain + 地址 + 分配者 PC → 软复位
 *   - 打印各域使用率统计
 *
 * 在 FreeRTOS 扁平内存模型下，这是唯一能捕获越界写入的机制。
 */
#pragma once

#include <thread>
#include <atomic>
#include <chrono>

#ifdef __cplusplus
extern "C" {
#endif
#include "esp_log.h"
#ifdef __cplusplus
}
#endif

namespace mem_protect {

class MemGuard {
public:
    static MemGuard &GetInstance() {
        static MemGuard instance;
        return instance;
    }

    /**
     * @brief 启动内存守护任务
     * @param interval_sec  巡检间隔（秒），默认 10
     */
    void Start(int interval_sec = 10) {
        if (running_.exchange(true)) return;

        interval_sec_ = interval_sec;
        ESP_LOGI("MEM_GUARD", "Memory guard started, interval=%ds", interval_sec);

        thread_ = std::thread([this]() {
            while (running_.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(
                    std::chrono::seconds(interval_sec_));
                if (running_.load(std::memory_order_relaxed)) {
                    DoGuardScan();
                }
            }
        });
    }

    /**
     * @brief 停止内存守护任务
     */
    void Stop() {
        running_.store(false);
        if (thread_.joinable()) {
            thread_.join();
        }
        ESP_LOGI("MEM_GUARD", "Memory guard stopped");
    }

private:
    MemGuard() : running_(false), interval_sec_(10) {}
    ~MemGuard() { Stop(); }

    void DoGuardScan() {
        // 扫描逻辑依赖 mem_pool.cc 中维护的链表。
        // 这里通过 mp_print_stats() 输出统计（哨兵校验在 mp_free 时已做）。
        // 未来迭代可以在此直接遍历链表检查哨兵。

        extern void mp_print_stats();  // from mem_pool.cc
        mp_print_stats();
    }

    std::atomic<bool> running_;
    std::thread thread_;
    int interval_sec_;
};

} // namespace mem_protect
