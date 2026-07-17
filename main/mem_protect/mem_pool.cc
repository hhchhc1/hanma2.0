/**
 * @file mem_pool.cc
 * @brief 域分区内存池实现
 */
#include "mem_pool.h"
#include <cstdlib>
#include <cstring>
#include <mutex>

#define TAG "MEM_POOL"

namespace mem_protect {

// ============================================================
// 分配块头部 — 每个分配块前面插此结构体
// ============================================================
struct BlockHeader {
    uint32_t magic_start;   ///< 起始魔数 0xDEADBEEF
    Domain   domain;        ///< 所属域
    size_t   size;          ///< 用户请求大小
    void    *caller;        ///< 保留（调用者地址，用于调试）
    BlockHeader *next;      ///< 链表指针（用于守护任务遍历）
    // ... 用户数据 ...
    // uint32_t magic_end;   ///< 结尾魔数 0xBEEFDEAD (写在用户数据之后)
};

static const uint32_t kMagicStart = 0xDEADBEEF;
static const uint32_t kMagicEnd   = 0xBEEFDEAD;

// 各域的活跃分配链表头
static BlockHeader *s_head[static_cast<int>(Domain::DOMAIN_COUNT)] = {};
static size_t s_alloc_count[static_cast<int>(Domain::DOMAIN_COUNT)] = {};
static size_t s_alloc_bytes[static_cast<int>(Domain::DOMAIN_COUNT)] = {};
static std::mutex s_mutex;

// ---- 内部函数 ----

static inline uint32_t *get_end_magic(BlockHeader *hdr) {
    auto *user_end = reinterpret_cast<uint8_t *>(hdr) + sizeof(BlockHeader) + hdr->size;
    return reinterpret_cast<uint32_t *>(user_end);
}

static BlockHeader *find_block(Domain domain, void *user_ptr) {
    int idx = static_cast<int>(domain);
    BlockHeader *hdr = reinterpret_cast<BlockHeader *>(
        static_cast<uint8_t *>(user_ptr) - sizeof(BlockHeader));
    // 回链确认
    for (BlockHeader *cur = s_head[idx]; cur; cur = cur->next) {
        if (cur == hdr) return cur;
    }
    return nullptr;
}

static void *alloc_internal(Domain domain, size_t size) {
    size_t total = sizeof(BlockHeader) + size + sizeof(uint32_t);
    void *raw = heap_caps_malloc(total, MALLOC_CAP_SPIRAM);
    if (!raw) {
        ESP_LOGE(TAG, "OOM: domain=%s req=%u total=%u",
                 kDomainNames[static_cast<int>(domain)], (unsigned)size, (unsigned)total);
        return nullptr;
    }

    auto *hdr = static_cast<BlockHeader *>(raw);
    hdr->magic_start = kMagicStart;
    hdr->domain = domain;
    hdr->size = size;
    hdr->caller = nullptr;  // caller tracking disabled (RISC-V frame-address limitation)

    *get_end_magic(hdr) = kMagicEnd;

    // 插入链表
    int idx = static_cast<int>(domain);
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        hdr->next = s_head[idx];
        s_head[idx] = hdr;
        s_alloc_count[idx]++;
        s_alloc_bytes[idx] += size;
    }

    return static_cast<uint8_t *>(raw) + sizeof(BlockHeader);
}

// ---- 公开接口 ----

void mp_init() {
    ESP_LOGI(TAG, "Memory pool system initialized, %d domains total",
             (int)Domain::DOMAIN_COUNT);
    for (int i = 0; i < (int)Domain::DOMAIN_COUNT; i++) {
        ESP_LOGI(TAG, "  %-10s : %u KB", kDomainNames[i],
                 (unsigned)(kDomainSizes[i] / 1024));
    }
}

void *mp_malloc(Domain domain, size_t size) {
    return alloc_internal(domain, size);
}

void *mp_calloc(Domain domain, size_t count, size_t size) {
    void *p = alloc_internal(domain, count * size);
    if (p) std::memset(p, 0, count * size);
    return p;
}

void *mp_realloc(Domain domain, void *ptr, size_t new_size) {
    if (!ptr) return alloc_internal(domain, new_size);
    if (new_size == 0) { mp_free(domain, ptr); return nullptr; }

    auto *hdr = reinterpret_cast<BlockHeader *>(
        static_cast<uint8_t *>(ptr) - sizeof(BlockHeader));

    if (hdr->domain != domain) {
        ESP_LOGE(TAG, "realloc domain mismatch: ptr=%s new=%s",
                 kDomainNames[static_cast<int>(hdr->domain)],
                 kDomainNames[static_cast<int>(domain)]);
        return nullptr;
    }

    void *newp = alloc_internal(domain, new_size);
    if (newp) {
        size_t copy = (hdr->size < new_size) ? hdr->size : new_size;
        std::memcpy(newp, ptr, copy);
        mp_free(domain, ptr);
    }
    return newp;
}

void mp_free(Domain domain, void *ptr) {
    if (!ptr) return;

    auto *hdr = reinterpret_cast<BlockHeader *>(
        static_cast<uint8_t *>(ptr) - sizeof(BlockHeader));

    // ========== 哨兵校验 ==========
    if (hdr->magic_start != kMagicStart) {
        ESP_LOGE(TAG, "CANARY START CORRUPTED: domain=%s ptr=%p magic=0x%08X",
                 kDomainNames[static_cast<int>(domain)], ptr, hdr->magic_start);
        return;
    }
    if (*get_end_magic(hdr) != kMagicEnd) {
        ESP_LOGE(TAG, "CANARY END CORRUPTED: domain=%s ptr=%p size=%u — BUFFER OVERFLOW!",
                 kDomainNames[static_cast<int>(domain)], ptr, (unsigned)hdr->size);
        return;
    }
    if (hdr->domain != domain) {
        ESP_LOGE(TAG, "DOMAIN MISMATCH: free(%s) on ptr allocated from %s",
                 kDomainNames[static_cast<int>(domain)],
                 kDomainNames[static_cast<int>(hdr->domain)]);
        return;
    }

    // ========== 防止 use-after-free: 填充 0xCD ==========
    std::memset(ptr, 0xCD, hdr->size);
    *get_end_magic(hdr) = 0;

    // ========== 从链表移除 ==========
    int idx = static_cast<int>(domain);
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_head[idx] == hdr) {
            s_head[idx] = hdr->next;
        } else {
            for (BlockHeader *cur = s_head[idx]; cur; cur = cur->next) {
                if (cur->next == hdr) {
                    cur->next = hdr->next;
                    break;
                }
            }
        }
        s_alloc_count[idx]--;
        s_alloc_bytes[idx] -= hdr->size;
    }

    hdr->magic_start = 0;
    heap_caps_free(hdr);
}

void mp_print_stats() {
    std::lock_guard<std::mutex> lock(s_mutex);
    ESP_LOGI(TAG, "=== Memory Pool Stats ===");
    for (int i = 0; i < (int)Domain::DOMAIN_COUNT; i++) {
        ESP_LOGI(TAG, "  %-10s : %6u KB used, %3u blocks",
                 kDomainNames[i],
                 (unsigned)(s_alloc_bytes[i] / 1024),
                 (unsigned)s_alloc_count[i]);
    }
}

size_t mp_get_usage(Domain domain) {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_alloc_bytes[static_cast<int>(domain)];
}

size_t mp_get_block_count(Domain domain) {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_alloc_count[static_cast<int>(domain)];
}

} // namespace mem_protect
