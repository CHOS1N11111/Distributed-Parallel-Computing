#include "common.h"
#include "net.h"
#include "cpu_ops.h"
#include "cpu_sort.h"

#include <iostream>
#include <exception>
#include <iomanip>

#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include <cmath>
/**
 * @file master.cpp
 * @brief 主节点 (Master) 入口程序
 * * 程序的控制中心。主要职责包括：
 * 1. 执行单机基准测试 (Baseline)，记录 sum/max/sort 的耗时。
 * 2. 作为 TCP 客户端连接 Worker 节点。
 * 3. 任务分发：将约 70% 的计算任务量通过网络下发给 Worker。
 * 4. 结果汇总：计算本地任务结果，接收 Worker 结果，并进行聚合（求和、比较最大值、归并排序）。
 * 5. 性能统计：计算并打印双机协同工作的总耗时与加速效果。
 */

// 按 3:7 切分任务规模，避免 master 或 worker 分到 0 个元素//
static inline uint64_t split_mid_30_70(uint64_t totalN) {
    // master: [0, mid) 约 30%//
    // worker: [mid, totalN) 约 70%//
    uint64_t mid = totalN * 3 / 10;
    if (mid == 0) mid = 1;
    if (mid >= totalN) mid = totalN - 1;
    return mid;
}

// ========== 单机计算接口 ==========//
// 对 data 做 log(sqrt(x)) 后求和//
float sum(const float data[], const int len) {
    double s = 0.0;
    for (int i = 0; i < len; ++i) s += logf(sqrtf(data[i]));
    return (float)s;
}

// 对 data 做 log(sqrt(x)) 后取最大值//
float max_function(const float data[], const int len) {
    float m = -INFINITY;
    for (int i = 0; i < len; ++i) {
        float v = logf(sqrtf(data[i]));
        m = (v > m ? v : m);
    }
    return m;
}

// 作业要求的接口命名//
float max(const float data[], const int len) {
    return max_function(data, len);
}


// 排序后写入 result，元素内容为 log(sqrt(x))//
float sort(const float data[], const int len, float result[]) {
    std::vector<float> tmp(data, data + len);
    if (len > 1) quicksort_by_key(tmp.data(), 0, len - 1);
    for (int i = 0; i < len; ++i) result[i] = logf(sqrtf(tmp[i]));
    return 0.0f;
}

// ========== 双机协同接口 ==========
// 只要保证 data/len 合法即可：
// - 当 data 指向 data[0..len) 时，直接用 sum/max/sort 计算
// - 当 data 为空时，master 根据 (begin,end) 生成序列，worker 也按协商范围自行生成

// 生成 [begin, end) 的递增数据//
static void init_local(std::vector<float>& data, uint64_t begin, uint64_t end) {
    uint64_t n = end - begin;
    data.resize((size_t)n);
    for (uint64_t i = 0; i < n; ++i) data[(size_t)i] = (float)(begin + i + 1);
}

// 确保 Winsock 只初始化一次//
static inline void ensure_wsa_inited() {
    static WsaInit wsa;
    (void)wsa;
}

// 单例形式持有 worker 端 socket//
static SOCKET& worker_sock_ref() {
    static SOCKET s = INVALID_SOCKET;
    return s;
}

// 修改这里即可替换 worker(B) 的 IP，单机自测可用 127.0.0.1//
const char* WORKER_IP = "127.0.0.1"; // TODO: worker IP    //192.168.71.1    //192.168.137.5  //本机测试： 127.0.0.1

// 取到已连接的 worker socket，必要时建立连接//
static SOCKET get_worker_sock() {
    SOCKET& s = worker_sock_ref();
    if (s == INVALID_SOCKET) {
        s = tcp_connect(WORKER_IP, PORT);
    }
    return s;
}

// 关闭并重置 worker socket，供下次重连//
static void reset_worker_sock() {
    SOCKET& s = worker_sock_ref();
    if (s != INVALID_SOCKET) {
        close_sock(s);
        s = INVALID_SOCKET;
    }
}

// 双机版 sum：master 计算前半段，worker 计算后半段再求和//
float sumSpeedUp(const float data[], const int len) {
    ensure_wsa_inited();
    if (len <= 0) return 0.0f;

    const uint64_t totalN = (uint64_t)len;
    const uint64_t mid = totalN / 2; // TODO: 可切换为 split_mid_30_70(totalN)
    //const uint64_t mid = split_mid_30_70(totalN);

    // 优先使用用户给的数据//
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
        // Worker 不可用则退化为单机
        return sum(aPtr, (int)aN);
    }

    // 通知 Worker 处理 [mid, totalN)
    MsgHeader h{ MAGIC, (uint32_t)Op::SUM, (uint64_t)(totalN - mid), mid, totalN };
    if (!send_all(c, &h, sizeof(h))) {
        reset_worker_sock();
        return sum(aPtr, (int)aN);
    }

    // 本地计算前半段
    //float aPart = cpu_sum_log_sqrt(aPtr, aN);//
    //float aPart = cpu_sum_log_sqrt_sse(aPtr, (uint64_t)aN);//
    float aPart = cpu_sum_log_sqrt_sse_omp(aPtr, (uint64_t)aN);



    
    // 等待 Worker 的结果//
    float bPart = 0.0f;
    if (!recv_all(c, &bPart, sizeof(bPart))) {
        reset_worker_sock();
        return aPart;
    }

    return aPart + bPart;
}

// 双机版 max：master/worker 各算一半，最后取较大值//
float maxSpeedUp(const float data[], const int len) {
    ensure_wsa_inited();
    if (len <= 0) return -INFINITY;

    const uint64_t totalN = (uint64_t)len;
    const uint64_t mid = totalN / 2; // TODO: 可切换为 split_mid_30_70(totalN)
    //const uint64_t mid = split_mid_30_70(totalN);

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
    //
    // 通知 Worker 处理 [mid, totalN)//
    MsgHeader h{ MAGIC, (uint32_t)Op::MAX, (uint64_t)(totalN - mid), mid, totalN };
    if (!send_all(c, &h, sizeof(h))) {
        reset_worker_sock();
        return max_function(aPtr, (int)aN);
    }

    // 本地计算前半段//
    //float aMax = cpu_max_log_sqrt(aPtr, aN);//
    //float aMax = cpu_max_log_sqrt_sse(aPtr, (uint64_t)aN);//
    float aMax = cpu_max_log_sqrt_sse_omp(aPtr, (uint64_t)aN);



    // 等待 Worker 的结果//
    float bMax = -INFINITY;
    if (!recv_all(c, &bMax, sizeof(bMax))) {
        reset_worker_sock();
        return aMax;
    }

    return (aMax > bMax ? aMax : bMax);
}

// 双机版 sort：两端各自排序后再归并，result 写入 log(sqrt(.))//
float sortSpeedUp(const float data[], const int len, float result[]) {
    ensure_wsa_inited();
    if (len <= 0 || !result) return 0.0f;

    const uint64_t totalN = (uint64_t)len;
    const uint64_t mid = totalN / 2; // TODO: 可切换为 split_mid_30_70(totalN)
    //const uint64_t mid = split_mid_30_70(totalN);

    // 尽可能直接复用用户数据//
    std::vector<float> localA;
    if (data && (uint64_t)len >= mid) {
        localA.assign(data, data + mid);
    }
    else {
        init_local(localA, 0, mid);
    }

    SOCKET c = get_worker_sock();
    if (c == INVALID_SOCKET) {
        // Worker 不可用时，全量单机排序//
        std::vector<float> full;
        if (data && (uint64_t)len >= totalN) full.assign(data, data + totalN);
        else init_local(full, 0, totalN);

        if (full.size() > 1) quicksort_by_key(full.data(), 0, (int64_t)full.size() - 1);
        for (int i = 0; i < len; ++i) result[i] = key_log_sqrt(full[(size_t)i]);
        return 0.0f;
    }
    
    
    MsgHeader h{ MAGIC, (uint32_t)Op::SORT, (uint64_t)(totalN - mid), mid, totalN };
    if (!send_all(c, &h, sizeof(h))) {
        reset_worker_sock();
        return sortSpeedUp(data, len, result); // Fallback 重试连接 Worker
    }

    // 等待 Worker 返回排序结果的字节数//
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

    // 本地乱序一次后按 key 排序，避免与 Worker 排序完全一致//
    shuffle_fisher_yates(localA.data(), (uint64_t)localA.size(), 0x1234ULL);
    if (localA.size() > 1) quicksort_by_key(localA.data(), 0, (int64_t)localA.size() - 1);

    // 归并后直接向 result 写入 log(sqrt(.)) 结果//
    merge_to_transformed(
        localA.data(), (int64_t)localA.size(),
        sortedB.data(), (int64_t)sortedB.size(),
        result
    );

    return 0.0f;
}

// ====== 从这里开始运行 main ======
// 获取 QueryPerformanceCounter 的倒频率（ms）//
static double freqInvMs() {
    static double v = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return 1000.0 / (double)f.QuadPart;
        }();
    return v;
}

// 主流程：单机基线、双机性能和结果校验//
int main() {

    try {

        
        // 单机测试（5 次取平均值）//
        const int N = (int)DATANUM;
        std::vector<float> raw;
        init_local(raw, 0, (uint64_t)N);  //

        auto run5_avg_ms = [&](auto&& fn) {
            double total = 0;
            for (int t = 0; t < 5; ++t) {
                LARGE_INTEGER st, ed;
                QueryPerformanceCounter(&st);
                fn();
                QueryPerformanceCounter(&ed);
                total += (ed.QuadPart - st.QuadPart) * freqInvMs();
            }
            return total / 5.0;
            };

        std::vector<float> out((size_t)N);

        double t_sum_base = run5_avg_ms([&] { (void)sum(raw.data(), N); });
        double t_max_base = run5_avg_ms([&] { (void)max(raw.data(), N); });

        shuffle_fisher_yates(raw.data(), (uint64_t)raw.size(), 0x20251216ULL); // 增加洗牌次数
        double t_sort_base = run5_avg_ms([&] { (void)sort(raw.data(), N, out.data()); });

        std::cout << "[BASE][RUN5_AVG][SUM ] avg=" << t_sum_base << " ms\n";
        std::cout << "[BASE][RUN5_AVG][MAX ] avg=" << t_max_base << " ms\n";
        std::cout << "[BASE][RUN5_AVG][SORT] avg=" << t_sort_base << " ms\n";
        std::cout << "\n";
        double t_total_base = t_sum_base + t_max_base + t_sort_base;
        std::cout << "[BASE][RUN5_AVG][TOTAL] elapsed=" << t_total_base << " ms\n";
        std::cout << "\n";
        
        /*
        // 单机测试（只测一次）
        const int N = (int)DATANUM;
        std::vector<float> raw;
        init_local(raw, 0, (uint64_t)N);

        auto run1_ms = [&](auto&& fn) {
            LARGE_INTEGER st, ed;
            QueryPerformanceCounter(&st);
            fn();
            QueryPerformanceCounter(&ed);
            return (ed.QuadPart - st.QuadPart) * freqInvMs();
            };

        std::vector<float> out((size_t)N);

        double t_sum_base = run1_ms([&] { (void)sum(raw.data(), N); });
        double t_max_base = run1_ms([&] { (void)max(raw.data(), N); });

        shuffle_fisher_yates(raw.data(), (uint64_t)raw.size(), 0x20251216ULL); // 增加洗牌次数

        double t_sort_base = run1_ms([&] { (void)sort(raw.data(), N, out.data()); });

        std::cout << "[BASE][RUN1][SUM ] elapsed=" << t_sum_base << " ms\n";
        std::cout << "[BASE][RUN1][MAX ] elapsed=" << t_max_base << " ms\n";
        std::cout << "[BASE][RUN1][SORT] elapsed=" << t_sort_base << " ms\n";
        std::cout << "\n";
        double t_total_base = t_sum_base + t_max_base + t_sort_base;
        std::cout << "[BASE][RUN1][TOTAL] elapsed=" << t_total_base << " ms\n";
        std::cout << "\n";
        */

        /*
        // 单次双机测试
        double t_sum_dual = 0, t_max_dual = 0, t_sort_dual = 0;
        {
            LARGE_INTEGER st, ed;
            QueryPerformanceCounter(&st);
            float ans = sumSpeedUp(nullptr, N);
            QueryPerformanceCounter(&ed);
            t_sum_dual = (ed.QuadPart - st.QuadPart) * freqInvMs();
            std::cout << "[SUM] result=" << ans << " elapsed=" << t_sum_dual << " ms\n";
        }

        {
            LARGE_INTEGER st, ed;
            QueryPerformanceCounter(&st);
            float ans = maxSpeedUp(nullptr, N);
            QueryPerformanceCounter(&ed);
            t_max_dual = (ed.QuadPart - st.QuadPart) * freqInvMs();
            std::cout << "[MAX] result=" << ans << " elapsed=" << t_max_dual << " ms\n";
        }

        {
            std::vector<float> out((size_t)N);
            LARGE_INTEGER st, ed;
            QueryPerformanceCounter(&st);
            sortSpeedUp(nullptr, N, out.data());
            QueryPerformanceCounter(&ed);
            std::cout << std::fixed << std::setprecision(10);
            t_sort_dual = (ed.QuadPart - st.QuadPart) * freqInvMs();
            std::cout << "[SORT] done elapsed=" << t_sort_dual << " ms\n";

            int midIndex = N / 2;

            // 简单校验输出
            // 前 5 个
            std::cout << "out[0..4]:\n ";
            for (int i = 0; i < 5 && i < N; ++i) std::cout << out[i] << (i == 4 ? '\n' : ' ');

            // 中间 5 个
            std::cout << "out[mid-2..mid+2]:\n ";
            int L = imax(0, midIndex - 2);
            int R = imin(N - 1, midIndex + 2);
            for (int i = L; i <= R; ++i) std::cout << out[i] << (i == R ? '\n' : ' ');

            // 最后 5 个
            std::cout << "out[last-4..last]:\n";
            int start = imax(0, N - 5);
            for (int i = start; i < N; ++i) std::cout << out[i] << (i == N - 1 ? '\n' : ' ');


        }

        std::cout << "\n";
        double t_total_dual = t_sum_dual + t_max_dual + t_sort_dual;
        std::cout << "[DUAL][TOTAL] elapsed=" << t_total_dual << " ms\n";
        std::cout << "\n";
        */

        // 双机测试取平均//
        double t_sum_dual_avg = 0, t_max_dual_avg = 0, t_sort_dual_avg = 0;

        // 记录最后一次的结果//
        float sum_ans = 0.0f;
        float max_ans = 0.0f;

        std::vector<float> out_dual((size_t)N);

        t_sum_dual_avg = run5_avg_ms([&] {
            sum_ans = sumSpeedUp(nullptr, N);
            });

        t_max_dual_avg = run5_avg_ms([&] {
            max_ans = maxSpeedUp(nullptr, N);
            });

        t_sort_dual_avg = run5_avg_ms([&] {
            sortSpeedUp(nullptr, N, out_dual.data());
            // 访问一次结果，确保 out_dual 在 Release 下不会被认为未使用//
            volatile float guard = out_dual[0];
            (void)guard;
            });

        // 计时结果输出（均值）//
        std::cout << "[DUAL][RUN5_AVG][SUM ] result=" << sum_ans << " avg=" << t_sum_dual_avg << " ms\n";
        std::cout << "[DUAL][RUN5_AVG][MAX ] result=" << max_ans << " avg=" << t_max_dual_avg << " ms\n";
        std::cout << "[DUAL][RUN5_AVG][SORT] done   avg=" << t_sort_dual_avg << " ms\n";

        double t_total_dual_avg = t_sum_dual_avg + t_max_dual_avg + t_sort_dual_avg;
        std::cout << "[DUAL][RUN5_AVG][TOTAL] avg=" << t_total_dual_avg << " ms\n\n";

        // 单次结果抽检打印（不放进 run5 里）//
        std::cout << std::fixed << std::setprecision(10);
        int midIndex = N / 2;

        // 前 5 个//
        std::cout << "out[0..4]:\n ";
        for (int i = 0; i < 5 && i < N; ++i) std::cout << out_dual[i] << (i == 4 ? '\n' : ' ');

        // 中间 5 个//
        std::cout << "out[mid-2..mid+2]:\n ";
        int L = imax(0, midIndex - 2);
        int R = imin(N - 1, midIndex + 2);
        for (int i = L; i <= R; ++i) std::cout << out_dual[i] << (i == R ? '\n' : ' ');

        // 最后 5 个//
        std::cout << "out[last-4..last]:\n";
        int start = imax(0, N - 5);
        for (int i = start; i < N; ++i) std::cout << out_dual[i] << (i == N - 1 ? '\n' : ' ');


        // 结束时断开 socket，配合 worker.cpp//
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

    system("pause");

}
