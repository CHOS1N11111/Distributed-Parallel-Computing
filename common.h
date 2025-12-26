/**
 * @file common.h
 * @brief 公共定义与工具模块
 * * 该文件被 Master 和 Worker 共同包含，定义了系统的基础配置和通信协议。
 * 主要内容：
 * 1. 数据规模 (DATANUM) 与 线程/分块常数。
 * 2. 通信协议结构体 (MsgHeader) 与 操作码枚举 (Op)。
 * 3. 网络配置 (端口号、魔数)。
 * 4. 通用工具函数：伪随机数生成器 (RNG) 和 Fisher-Yates 洗牌算法，确保两端数据生成一致。
 */
#pragma once

#include <cstdint>

#define MAX_THREADS 64
// 每个线程默认处理的元素数，作为基准块大小
#define SUBDATANUM 2000000  // TODO：可以更改SUBDATANUM
// 内置基准总量：64 * 2,000,000 = 128,000,000 个 float
#define DATANUM (SUBDATANUM * MAX_THREADS)

// 预留：若 worker 需要回连 master 时使用
// static constexpr const char* MASTER_IP = "0.0.0.0"; // 
// 主从通信共用的 TCP 端口
static constexpr uint16_t PORT = 50001;       //  TODO：可以更改端口

enum class Op : uint32_t {
    SUM = 1,
    MAX = 2,
    SORT = 3
};

// 这两个max和min函数仅用于打印排序结果示例，不参与核心计算
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int imin(int a, int b) { return a < b ? a : b; }
//

// 网络协议：先发头部，再按需要发送 payload
#pragma pack(push, 1)
struct MsgHeader {
    uint32_t magic;     // 'DPCT'
    uint32_t op;        // 对应 Op 枚举
    uint64_t len;       // worker 需要处理的 float 数量（end - begin）
    uint64_t begin;     // 负责的全局起始下标（含）
    uint64_t end;       // 负责的全局结束下标（不含）
};
#pragma pack(pop)

static constexpr uint32_t MAGIC = 0x54435044; // 'DPCT'

// ===== shuffle (Fisher–Yates) =====
//伪随机数生成器
static inline uint64_t rng_next_u64(uint64_t& s) {
    // xorshift64*，够用且快；固定 seed 可复现实验
    s ^= (s >> 12);
    s ^= (s << 25);
    s ^= (s >> 27);
    return s * 2685821657736338717ULL;
}

//Fisher–Yates 洗牌算法
//seed不同，打乱结果不同
static inline void shuffle_fisher_yates(float* a, uint64_t n, uint64_t seed = 0xC0FFEE123456789ULL) {
    if (!a || n < 2) return;
    uint64_t s = seed;
    for (uint64_t i = n - 1; i > 0; --i) {
        uint64_t j = rng_next_u64(s) % (i + 1);
        float t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

