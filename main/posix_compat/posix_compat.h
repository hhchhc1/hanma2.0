/**
 * @file posix_compat.h
 * @brief POSIX 兼容层聚合头文件
 *
 * 在项目的任何 .cc/.c 文件中 #include "posix_compat/posix_compat.h"
 * 即可获得全部 POSIX 标准接口替代。
 *
 * 参考 OpenVela (NuttX) POSIX 标准分层设计。
 */
#pragma once

#include "posix_task.h"
#include "posix_event.h"
#include "posix_queue.h"
#include "posix_timer.h"

// ============================================================
// vTaskDelay → POSIX 标准 sleep 的便捷宏
// ============================================================
#include <thread>
#include <chrono>

// 直接用 std::this_thread::sleep_for 替代 vTaskDelay
// 用法: POSIX_SLEEP_MS(100) 替代 vTaskDelay(pdMS_TO_TICKS(100))
#define POSIX_SLEEP_MS(ms) \
    std::this_thread::sleep_for(std::chrono::milliseconds(ms))

// ============================================================
// 编译期断言：确保在 ESP-IDF pthread 环境下编译
// ============================================================
#if !defined(CONFIG_PTHREAD_TASK_NAME_DEFAULT) && !defined(UNIT_TEST)
#warning "POSIX compat layer: Consider enabling CONFIG_PTHREAD_TASK_NAME_DEFAULT in sdkconfig for better debug output"
#endif
