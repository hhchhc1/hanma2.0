/**
 * @file mem_pool.h
 * @brief 域分区内存池 — 轻量级内存隔离
 *
 * 将 PSRAM 划分为独立的"内存域"，每个模块只能在自己的域内分配。
 * 调用方传错 domain → 编译期/运行期拒绝，防止跨模块内存踩踏。
 *
 * ESP32-P4 没有 MMU，做不到真正的虚拟内存隔离。
 * 此框架通过软件层面的域划分，提供"逻辑隔离"。
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_heap_caps.h"
#include "esp_log.h"

#ifdef __cplusplus
}
#endif

namespace mem_protect {

/**
 * @brief 内存域定义（5 个隔离域）
 *
 * 每个域从 PSRAM 预分配独立空间，域之间指针不互通。
 */
enum class Domain : uint8_t {
    DOMAIN_SYSTEM   = 0,  ///< 系统核心: 状态机、配置、MCP 消息
    DOMAIN_AUDIO    = 1,  ///< 音频管线: Opus 编解码、PCM 队列
    DOMAIN_DISPLAY  = 2,  ///< 显示系统: LVGL 对象、图片缓存
    DOMAIN_CAMERA   = 3,  ///< 摄像头: 帧缓冲、运动检测
    DOMAIN_NETWORK  = 4,  ///< 网络: WebSocket 缓冲、HTTP 请求

    DOMAIN_COUNT    = 5
};

/** 每个域的大小（字节），可根据实际需求调整 */
constexpr size_t kDomainSizes[] = {
    128 * 1024,   // DOMAIN_SYSTEM   (128 KB)
    512 * 1024,   // DOMAIN_AUDIO    (512 KB)
    256 * 1024,   // DOMAIN_DISPLAY  (256 KB)
    512 * 1024,   // DOMAIN_CAMERA   (512 KB)
    128 * 1024,   // DOMAIN_NETWORK  (128 KB)
};

/** 域名，用于日志输出 */
constexpr const char *kDomainNames[] = {
    "SYSTEM", "AUDIO", "DISPLAY", "CAMERA", "NETWORK"
};

/**
 * @brief 从指定域分配内存
 *
 * @param domain  内存域
 * @param size    请求大小 (字节)
 * @return 分配成功返回指针，失败返回 nullptr
 *
 * 特性:
 * - 自动从 PSRAM 分配 (MALLOC_CAP_SPIRAM)
 * - 分配时在块头记录 domain 信息（用于审计）
 * - 调用方传 DOMAIN_AUDIO 的指针不能给 DOMAIN_DISPLAY 的 mp_free
 */
void *mp_malloc(Domain domain, size_t size);

/**
 * @brief 释放从指定域分配的内存
 *
 * @param domain  必须与分配时的 domain 一致
 * @param ptr     要释放的指针
 */
void mp_free(Domain domain, void *ptr);

/**
 * @brief 从指定域分配并清零
 */
void *mp_calloc(Domain domain, size_t count, size_t size);

/**
 * @brief 重新分配
 */
void *mp_realloc(Domain domain, void *ptr, size_t new_size);

/**
 * @brief 初始化内存池系统（应在 main() 早期调用）
 */
void mp_init();

/**
 * @brief 打印各域内存使用统计
 */
void mp_print_stats();

/**
 * @brief 获取指定域当前使用量（字节）
 */
size_t mp_get_usage(Domain domain);

/**
 * @brief 获取指定域当前活跃分配块数
 */
size_t mp_get_block_count(Domain domain);

} // namespace mem_protect
