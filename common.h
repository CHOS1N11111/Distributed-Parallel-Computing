#pragma once

#include <cstdint>

#define MAX_THREADS 64
#define SUBDATANUM 2000000
#define DATANUM (SUBDATANUM * MAX_THREADS)

static constexpr const char* MASTER_IP = "192.168.1.10"; // TODO: 改成 Master(A机) IP
static constexpr uint16_t PORT = 50001;                  // TODO: 可改端口

enum class Op : uint32_t {
    SUM = 1,
    MAX = 2,
    SORT = 3
};

// 网络消息：先发 header，再发 payload（若有）
#pragma pack(push, 1)
struct MsgHeader {
    uint32_t magic;     // 'DPCT'
    uint32_t op;        // Op
    uint64_t len;       // 本次任务本地数据长度（float 个数）
    uint64_t begin;     // 全局起始下标（用于初始化值域）
    uint64_t end;       // 全局结束下标（开区间）
};
#pragma pack(pop)

static constexpr uint32_t MAGIC = 0x54435044; // 'DPCT'
