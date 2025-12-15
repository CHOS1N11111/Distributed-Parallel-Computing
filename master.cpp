#include "common.h"
#include "net.h"
#include "cpu_ops.h"
#include "cpu_sort.h"

#include <iostream>
#include <exception>

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

// ========== 汾޼٣ ==========
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

// Assignment-required interface name.
float max(const float data[], const int len) {
    return max_function(data, len);
}


float sort(const float data[], const int len, float result[]) {
    std::vector<float> tmp(data, data + len);
    if (len > 1) quicksort_by_key(tmp.data(), 0, len - 1);
    for (int i = 0; i < len; ++i) result[i] = logf(sqrtf(tmp[i]));
    return 0.0f;
}

// ========== ˫ٰ汾ṩĽӿڣ ==========
// ˵ҵҪ̨ԶԼݶΣ⴫䳬顣
// ﱣ data/len ƥӿڣ
// - ÷ data data[0..len) ãǱֱӸñݣsum/max sort ´ڱ򣩡
// -  data Ϊջ㣬 (begin,end) 򱾻ɱݣworker ͬİΡ

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

// ֻҪWorker(B)  IP
const char* WORKER_IP = "127.0.0.1"; // TODO: ĳ Worker IP

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

// -------- sumSpeedUp񻮷  Զִ  ش  ϲ --------
float sumSpeedUp(const float data[], const int len) {
    ensure_wsa_inited();
    if (len <= 0) return 0.0f;

    const uint64_t totalN = (uint64_t)len;
    const uint64_t mid = totalN / 2;

    // ָ루ʡڴ棩
    //  data ã򱾻 [0,mid)
    std::vector<float> localA;
    const float* aPtr = nullptr;
    int64_t aN = (int64_t)mid;

    if (data && (uint64_t)len >= mid) {
        aPtr = data; // ֱ data[0..mid)
    }
    else {
        init_local(localA, 0, mid);
        aPtr = localA.data();
        aN = (int64_t)localA.size();
    }

    SOCKET c = get_worker_sock();
    if (c == INVALID_SOCKET) {
        // Worker ã˻Ϊ㣨֤ȷ
        return sum(aPtr, (int)aN);
    }

    // · Worker  [mid, totalN) Ĳֺ
    MsgHeader h{ MAGIC, (uint32_t)Op::SUM, (uint64_t)(totalN - mid), mid, totalN };
    if (!send_all(c, &h, sizeof(h))) {
        reset_worker_sock();
        return sum(aPtr, (int)aN);
    }

    // 
    float aPart = cpu_sum_log_sqrt(aPtr, aN);

    //  Worker ϲ
    float bPart = 0.0f;
    if (!recv_all(c, &bPart, sizeof(bPart))) {
        reset_worker_sock();
        return aPart;
    }

    return aPart + bPart;
}

// -------- maxSpeedUp񻮷  Զִ  ش  ϲ --------
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

    // · Worker  [mid, totalN) ֵ
    MsgHeader h{ MAGIC, (uint32_t)Op::MAX, (uint64_t)(totalN - mid), mid, totalN };
    if (!send_all(c, &h, sizeof(h))) {
        reset_worker_sock();
        return max_function(aPtr, (int)aN);
    }

    // 
    float aMax = cpu_max_log_sqrt(aPtr, aN);

    //  Worker ϲ
    float bMax = -INFINITY;
    if (!recv_all(c, &bMax, sizeof(bMax))) {
        reset_worker_sock();
        return aMax;
    }

    return (aMax > bMax ? aMax : bMax);
}

// -------- sortSpeedUp񻮷  Զִ  ش  ϲ --------
// Լresult[] ǡȫ log(sqrt(.)) С㵱ǰ merge_to_transformed ƣ
float sortSpeedUp(const float data[], const int len, float result[]) {
    ensure_wsa_inited();
    if (len <= 0 || !result) return 0.0f;

    const uint64_t totalN = (uint64_t)len;
    const uint64_t mid = totalN / 2;

    // ΣҪԭؽпд   vectorΣ
    std::vector<float> localA;
    if (data && (uint64_t)len >= mid) {
        localA.assign(data, data + mid);
    }
    else {
        init_local(localA, 0, mid);
    }

    SOCKET c = get_worker_sock();
    if (c == INVALID_SOCKET) {
        // Worker ã˻Ϊȫ򣨱֤ȷ
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
        return sortSpeedUp(data, len, result); // һ fallbackߵ Worker ÷֧
    }

    //  Worker ش bytes
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

    // 򣨰 key
    if (localA.size() > 1) quicksort_by_key(localA.data(), 0, (int64_t)localA.size() - 1);

    // 鲢 result[] д log(sqrt(.)) 
    merge_to_transformed(
        localA.data(), (int64_t)localA.size(),
        sortedB.data(), (int64_t)sortedB.size(),
        result
    );

    return 0.0f;
}

// ====== ѡԼĲ mainʦӿʱԺ ======
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
		//单机测试
        const int N = (int)DATANUM;
        std::vector<float> raw;
        init_local(raw, 0, (uint64_t)N);  // 生成 1..N  (同你现有逻辑) :contentReference[oaicite:4]{index=4}

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
        double t_sort_base = run5_avg_ms([&] { (void)sort(raw.data(), N, out.data()); });

        std::cout << "[BASE][SUM ] avg=" << t_sum_base << " ms\n";
        std::cout << "[BASE][MAX ] avg=" << t_max_base << " ms\n";
        std::cout << "[BASE][SORT] avg=" << t_sort_base << " ms\n";


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

        // ر socketworker ˳㵱ǰ worker.cpp Ϊ
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