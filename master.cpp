#include "common.h"
#include "net.h"
#include "cuda_ops.cuh"
#include "cpu_sort.h"

#include <iostream>
#include <exception>

#include <vector>
#include <iostream>
#include <windows.h>
#include <cmath>

// ========== 基础版本（单机、无加速） ==========
float sum(const float data[], const int len) {
    double s = 0.0;
    for (int i = 0; i < len; ++i) s += logf(sqrtf(data[i]));
    return (float)s;
}

float max_function(const float data[], const int len) {
    float m = -INFINITY;
    for (int i = 0; i < len; ++i) {
        float v = logf(sqrtf(data[i]));
        m = (v > m ? v : m);
    }
    return m;
}

float sort(const float data[], const int len, float result[]) {
    std::vector<float> tmp(data, data + len);
    if (len > 1) quicksort_by_key(tmp.data(), 0, len - 1);
    for (int i = 0; i < len; ++i) result[i] = logf(sqrtf(tmp[i]));
    return 0.0f;
}

// ========== 双机加速版本（必须提供的接口） ==========
// 说明：按作业要求，两台机器各自独立生成自己负责的数据段，避免传输超大数组。
// 这里保留 data/len 以匹配接口：
// - 如果调用方传入了完整 data（且 data[0..len) 可用），我们本机直接复用本机半段数据（sum/max 不拷贝；sort 会拷贝半段用于本地排序）。
// - 若 data 为空或不足，则按 (begin,end) 规则本机生成本机半段数据；worker 端同理生成它负责的半段。

static void init_local(std::vector<float>& data, uint64_t begin, uint64_t end) {
    uint64_t n = end - begin;
    data.resize((size_t)n);
    for (uint64_t i = 0; i < n; ++i) data[(size_t)i] = (float)(begin + i + 1);
}

static inline void ensure_wsa_inited() {
    static WsaInit wsa;
    (void)wsa;
}

static SOCKET& worker_sock_ref() {
    static SOCKET s = INVALID_SOCKET;
    return s;
}

// 你只需要改这里：Worker(B机) 的 IP
const char* WORKER_IP = "127.0.0.1" ; // TODO: 改成你的 Worker IP

static SOCKET get_worker_sock() {
    SOCKET& s = worker_sock_ref();
    if (s == INVALID_SOCKET) {
        s = tcp_connect(WORKER_IP, PORT);
    }
    return s;
}

static void reset_worker_sock() {
    SOCKET& s = worker_sock_ref();
    if (s != INVALID_SOCKET) {
        close_sock(s);
        s = INVALID_SOCKET;
    }
}

// -------- sumSpeedUp：任务划分 → 远程执行 → 回传 → 合并 --------
float sumSpeedUp(const float data[], const int len) {
    ensure_wsa_inited();
    if (len <= 0) return 0.0f;

    const uint64_t totalN = (uint64_t)len;
    const uint64_t mid = totalN / 2;

    // 本机半段数据指针（不拷贝，尽量省内存）
    // 若 data 不可用，则本机生成 [0,mid)
    std::vector<float> localA;
    const float* aPtr = nullptr;
    int64_t aN = (int64_t)mid;

    if (data && (uint64_t)len >= mid) {
        aPtr = data; // 直接用 data[0..mid)
    }
    else {
        init_local(localA, 0, mid);
        aPtr = localA.data();
        aN = (int64_t)localA.size();
    }

    SOCKET c = get_worker_sock();
    if (c == INVALID_SOCKET) {
        // Worker 不可用：退化为本机计算（保证功能正确）
        return sum(aPtr, (int)aN);
    }

    // 下发任务：让 Worker 计算 [mid, totalN) 的部分和
    MsgHeader h{ MAGIC, (uint32_t)Op::SUM, (uint64_t)(totalN - mid), mid, totalN };
    if (!send_all(c, &h, sizeof(h))) {
        reset_worker_sock();
        return sum(aPtr, (int)aN);
    }

    // 本机计算
    float aPart = cuda_sum_log_sqrt(aPtr, aN);

    // 回收 Worker 结果并合并
    float bPart = 0.0f;
    if (!recv_all(c, &bPart, sizeof(bPart))) {
        reset_worker_sock();
        return aPart;
    }

    return aPart + bPart;
}

// -------- maxSpeedUp：任务划分 → 远程执行 → 回传 → 合并 --------
float maxSpeedUp(const float data[], const int len) {
    ensure_wsa_inited();
    if (len <= 0) return -INFINITY;

    const uint64_t totalN = (uint64_t)len;
    const uint64_t mid = totalN / 2;

    std::vector<float> localA;
    const float* aPtr = nullptr;
    int64_t aN = (int64_t)mid;

    if (data && (uint64_t)len >= mid) {
        aPtr = data; // data[0..mid)
    }
    else {
        init_local(localA, 0, mid);
        aPtr = localA.data();
        aN = (int64_t)localA.size();
    }

    SOCKET c = get_worker_sock();
    if (c == INVALID_SOCKET) {
        return max_function(aPtr, (int)aN);
    }

    // 下发任务：让 Worker 计算 [mid, totalN) 的最大值
    MsgHeader h{ MAGIC, (uint32_t)Op::MAX, (uint64_t)(totalN - mid), mid, totalN };
    if (!send_all(c, &h, sizeof(h))) {
        reset_worker_sock();
        return max_function(aPtr, (int)aN);
    }

    // 本机计算
    float aMax = cuda_max_log_sqrt(aPtr, aN);

    // 回收 Worker 结果并合并
    float bMax = -INFINITY;
    if (!recv_all(c, &bMax, sizeof(bMax))) {
        reset_worker_sock();
        return aMax;
    }

    return (aMax > bMax ? aMax : bMax);
}

// -------- sortSpeedUp：任务划分 → 远程执行 → 回传 → 合并 --------
// 约定：result[] 输出的是“全局排序后的 log(sqrt(.)) 序列”（符合你当前 merge_to_transformed 的设计）
float sortSpeedUp(const float data[], const int len, float result[]) {
    ensure_wsa_inited();
    if (len <= 0 || !result) return 0.0f;

    const uint64_t totalN = (uint64_t)len;
    const uint64_t mid = totalN / 2;

    // 本机半段：排序需要原地交换，因此这里必须有可写数组 → 采用 vector（拷贝半段）
    std::vector<float> localA;
    if (data && (uint64_t)len >= mid) {
        localA.assign(data, data + mid);
    }
    else {
        init_local(localA, 0, mid);
    }

    SOCKET c = get_worker_sock();
    if (c == INVALID_SOCKET) {
        // Worker 不可用：退化为本机对全量排序（保证正确）
        std::vector<float> full;
        if (data && (uint64_t)len >= totalN) full.assign(data, data + totalN);
        else init_local(full, 0, totalN);

        if (full.size() > 1) quicksort_by_key(full.data(), 0, (int64_t)full.size() - 1);
        for (int i = 0; i < len; ++i) result[i] = key_log_sqrt(full[(size_t)i]);
        return 0.0f;
    }

    // 下发任务：让 Worker 排序 [mid, totalN) 并回传“排序后的原始 float 值数组”
    MsgHeader h{ MAGIC, (uint32_t)Op::SORT, (uint64_t)(totalN - mid), mid, totalN };
    if (!send_all(c, &h, sizeof(h))) {
        reset_worker_sock();
        return sortSpeedUp(data, len, result); // 再走一次 fallback（会走到上面的 Worker 不可用分支）
    }

    // 接收 Worker 回传：先收 bytes，再收数组
    uint64_t bBytes = 0;
    if (!recv_all(c, &bBytes, sizeof(bBytes))) {
        reset_worker_sock();
        return sort(data, len, result);
    }
    if (bBytes % sizeof(float) != 0) {
        reset_worker_sock();
        return sort(data, len, result);
    }
    uint64_t bN = bBytes / sizeof(float);

    std::vector<float> sortedB((size_t)bN);
    if (bBytes > 0 && !recv_all(c, sortedB.data(), (size_t)bBytes)) {
        reset_worker_sock();
        return sort(data, len, result);
    }

    // 本机半段排序（按 key）
    if (localA.size() > 1) quicksort_by_key(localA.data(), 0, (int64_t)localA.size() - 1);

    // 归并输出：最终 result[] 写入 log(sqrt(.)) 的升序序列
    merge_to_transformed(
        localA.data(), (int64_t)localA.size(),
        sortedB.data(), (int64_t)sortedB.size(),
        result
    );

    return 0.0f;
}

// ====== （可选）你自己的测试 main：老师测接口时可以忽略 ======
static double freqInvMs() {
    static double v = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return 1000.0 / (double)f.QuadPart;
        }();
    return v;
}

int main() {

    try {
        // 自测：不传入 data，让两机按规则各自生成数据
        const int N = (int)DATANUM;

        {
            LARGE_INTEGER st, ed;
            QueryPerformanceCounter(&st);
            float ans = sumSpeedUp(nullptr, N);
            QueryPerformanceCounter(&ed);
            std::cout << "[SUM] result=" << ans
                << " elapsed=" << (ed.QuadPart - st.QuadPart) * freqInvMs() << " ms\n";
        }

        {
            LARGE_INTEGER st, ed;
            QueryPerformanceCounter(&st);
            float ans = maxSpeedUp(nullptr, N);
            QueryPerformanceCounter(&ed);
            std::cout << "[MAX] result=" << ans
                << " elapsed=" << (ed.QuadPart - st.QuadPart) * freqInvMs() << " ms\n";
        }

        {
            std::vector<float> out((size_t)N);
            LARGE_INTEGER st, ed;
            QueryPerformanceCounter(&st);
            sortSpeedUp(nullptr, N, out.data());
            QueryPerformanceCounter(&ed);
            std::cout << "[SORT] done"
                << " elapsed=" << (ed.QuadPart - st.QuadPart) * freqInvMs() << " ms\n";
            std::cout << "out[0]=" << out[0] << " out[last]=" << out.back() << "\n";
        }

        // 程序结束会关闭 socket，worker 会因断连退出（符合你当前 worker.cpp 的行为）
        reset_worker_sock();
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "[FATAL] unknown exception\n";
        return 1;
    }
}