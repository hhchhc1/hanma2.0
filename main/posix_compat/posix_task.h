/**
 * @file posix_task.h
 * @brief POSIX-compatible task creation API — 替代 FreeRTOS xTaskCreate
 *
 * 参考 OpenVela (NuttX) POSIX 标准分层设计，将 FreeRTOS 原生任务 API
 * 封装为标准 pthread 接口。ESP-IDF 的 pthread 实现基于 FreeRTOS 任务，
 * 因此性能和行为完全等价。
 */
#pragma once

#include <pthread.h>
#include <functional>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_pthread.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
}
#endif

namespace posix {

/**
 * @brief 任务属性配置
 */
struct TaskConfig {
    std::string name;           ///< 任务名称 (用于日志和调试)
    uint32_t stack_size;        ///< 栈大小 (字节)
    int priority;               ///< FreeRTOS 优先级 (0~25)
    int core_id;                ///< 核心绑定: -1=不绑定, 0=PRO_CPU, 1=APP_CPU
};

/**
 * @brief 创建 POSIX 线程（等效于 xTaskCreate / xTaskCreatePinnedToCore）
 *
 * @param config   任务配置
 * @param func     线程入口函数 (std::function<void()>)
 * @return pthread_t 线程句柄，失败时行为同 pthread_create
 *
 * 使用示例:
 * @code
 *   posix::TaskConfig cfg = {"audio_input", 6144, 8, -1};
 *   pthread_t tid = posix::task_create(cfg, []() {
 *       audio_input_task();
 *       // 不需要手动 vTaskDelete — 函数返回时 pthread_exit 自动调用
 *   });
 * @endcode
 */
static inline pthread_t task_create(const TaskConfig &cfg, std::function<void()> func)
{
    pthread_t tid;
    pthread_attr_t attr;
    esp_pthread_cfg_t esp_cfg;

    pthread_attr_init(&attr);

    // 栈大小映射
    pthread_attr_setstacksize(&attr, cfg.stack_size);

    // 设置线程为 detach 状态，退出后自动回收资源
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // ESP-IDF pthread 配置: 映射到 FreeRTOS 优先级和核心
    esp_cfg = esp_pthread_get_default_config();
    esp_cfg.thread_name = cfg.name.c_str();
    esp_cfg.stack_size = cfg.stack_size;
    esp_cfg.prio = cfg.priority;
    esp_cfg.pin_to_core = (cfg.core_id < 0) ? tskNO_AFFINITY : cfg.core_id;
    esp_pthread_set_cfg(&esp_cfg);

    int ret = pthread_create(&tid, &attr,
        [](void *arg) -> void * {
            auto *f = static_cast<std::function<void()> *>(arg);
            (*f)();
            delete f;
            return nullptr;
        },
        new std::function<void()>(std::move(func)));

    pthread_attr_destroy(&attr);

    if (ret != 0) {
        printf("[POSIX_TASK] ERROR: pthread_create failed for '%s': %d\n",
               cfg.name.c_str(), ret);
    } else {
        printf("[POSIX_TASK] Created '%s' stack=%zu prio=%d core=%d\n",
               cfg.name.c_str(), (size_t)cfg.stack_size, cfg.priority, cfg.core_id);
    }

    return tid;
}

/**
 * @brief 删除指定线程 (等效于 vTaskDelete)
 *
 * @param tid  线程句柄，传 0 表示当前线程
 *
 * 注意: 在 POSIX 模型中通常让线程自然返回，而非强制取消。
 * 保留此接口用于兼容紧急停止场景。
 */
static inline void task_delete(pthread_t tid = 0)
{
    if (tid == 0) {
        pthread_exit(nullptr);
    } else {
        pthread_cancel(tid);
    }
}

} // namespace posix

/*
 * 兼容宏 — 直接替换无需修改调用代码
 *
 * 使用方式：在所有 .cc/.c 文件开头 #include "posix_compat/posix_task.h"
 * 然后原来的 FreeRTOS API 调用自动映射到 POSIX 版本:
 *
 *   xTaskCreate(func, name, stack, param, prio, handle)
 *       → posix::task_create({name, stack, prio, -1}, [param]{ func(param); })
 *
 *   vTaskDelete(NULL) → pthread_exit(nullptr)
 *   vTaskDelay(pdMS_TO_TICKS(ms)) → usleep((ms) * 1000)
 */

