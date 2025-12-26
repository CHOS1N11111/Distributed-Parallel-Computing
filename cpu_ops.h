/**
 * @file cpu_ops.h
 * @brief 基础计算核心模块
 * * 该文件提供了针对 float 数据逐个计算 ln(sqrt(x)) 后的基础聚合操作实现。
 * 包含单机版的求和 (sum) 和求最大值 (max) 函数。
 * 这些函数是 Master 和 Worker 节点进行本地计算时的底层核心逻辑。
 */
#pragma once
#include <cmath>
#include <cstdint>

// ===== SSE (SIMD) 支持 =====
// 用于对 sqrt 做 4 路并行处理（_mm_sqrt_ps）。
// 说明：标准库 logf 通常是标量实现，因此这里采用“sqrt 向量化 + log 标量”的方式，
// 以保证结果与标量版本一致，同时满足作业中“使用 SSE”要求。
//
#if defined(USE_SSE)
  #if defined(_MSC_VER)
    #include <intrin.h>
  #endif
  #include <immintrin.h>
#endif

//OpenMP 支持
#ifdef USE_OPENMP
#include <omp.h>
#endif

// 计算 log(sqrt(x)) 的总和
inline float cpu_sum_log_sqrt(const float* data, uint64_t n) {
    double s = 0.0;
    for (uint64_t i = 0; i < n; ++i) {
        s += logf(sqrtf(data[i]));
    }
    return (float)s;
}

// 计算 log(sqrt(x)) 的最大值//
inline float cpu_max_log_sqrt(const float* data, uint64_t n) {
    float m = -INFINITY;
    for (uint64_t i = 0; i < n; ++i) {
        float v = logf(sqrtf(data[i]));
        m = (v > m ? v : m);
    }
    return m;
}

// ===== 单独 SSE 版本（用于 SpeedUp 路径） =====
// sqrt 使用 SSE 4 路并行，logf 仍为标量（保证与标量版一致）。
//
inline float cpu_sum_log_sqrt_sse(const float* data, uint64_t n) {
#if defined(USE_SSE)
    double s = 0.0;
    uint64_t i = 0;
    alignas(16) float buf[4];

    for (; i + 4 <= n; i += 4) {
        __m128 x = _mm_loadu_ps(data + i);
        __m128 r = _mm_sqrt_ps(x);
        _mm_store_ps(buf, r);
        s += logf(buf[0]) + logf(buf[1]) + logf(buf[2]) + logf(buf[3]);
    }
    for (; i < n; ++i) s += logf(sqrtf(data[i]));
    return (float)s;
#else
    return cpu_sum_log_sqrt(data, n);
#endif
}

inline float cpu_max_log_sqrt_sse(const float* data, uint64_t n) {
#if defined(USE_SSE)
    float m = -INFINITY;
    uint64_t i = 0;
    alignas(16) float buf[4];

    for (; i + 4 <= n; i += 4) {
        __m128 x = _mm_loadu_ps(data + i);
        __m128 r = _mm_sqrt_ps(x);
        _mm_store_ps(buf, r);
        for (int k = 0; k < 4; ++k) {
            float v = logf(buf[k]);
            m = (v > m ? v : m);
        }
    }
    for (; i < n; ++i) {
        float v = logf(sqrtf(data[i]));
        m = (v > m ? v : m);
    }
    return m;
#else
    return cpu_max_log_sqrt(data, n);
#endif
}

// -------------------- 组合版：OpenMP + SSE（同一个函数里同时用） --------------------
// 设计：
// - OpenMP：外层并行拆分数据块到多个线程
// - SSE：每个线程内部对 sqrt 做 4 路并行（_mm_sqrt_ps）
// - logf：仍是标量（保证与 baseline 结果一致）
// - 尾巴（n不是4倍数）串行补算一次

inline float cpu_sum_log_sqrt_sse_omp(const float* data, uint64_t n) {
#if defined(USE_OPENMP)
    // OpenMP 是“多线程并行”的工具：把大循环拆给多个线程同时做，让 CPU 多核一起工作。
    // sum 不能被多个线程同时写，因此要用 reduction 让每个线程先算自己的小和，最后再统一合并。
    double sum = 0.0;

#if defined(USE_SSE)
    const uint64_t n4 = (n / 4) * 4;

    // OpenMP：把 [0, n4) 这段循环平均分给多个线程做，相当于“多人分工”。
    // schedule(static) 表示每个线程拿到固定的一段任务，开销小且稳定。
#pragma omp parallel for reduction(+:sum) schedule(static)
    for (long long i = 0; i < (long long)n4; i += 4) {
        // 这里的并行分两层：OpenMP 负责“多线程分工”，SSE 负责“每个线程里一次算 4 个数”，两者互不冲突。
        __m128 x = _mm_loadu_ps(data + i);
        __m128 r = _mm_sqrt_ps(x);

        alignas(16) float buf[4];
        _mm_store_ps(buf, r);

        sum += logf(buf[0]) + logf(buf[1]) + logf(buf[2]) + logf(buf[3]);
    }

    // n 不是 4 的倍数时，尾部剩下的元素不进并行区，串行补算一次，避免被多个线程重复处理。

    // 尾巴补算（只算一次，避免并发重复）
    for (uint64_t i = n4; i < n; ++i) sum += logf(sqrtf(data[i]));
    return (float)sum;

#else
    // 没有 SSE 时，用 OpenMP 直接把整个循环并行化，reduction 仍然负责把各线程结果安全相加。
#pragma omp parallel for reduction(+:sum) schedule(static)
    for (long long i = 0; i < (long long)n; ++i) {
        sum += logf(sqrtf(data[i]));
    }
    return (float)sum;
#endif

#else
    // 没开 OpenMP：退化为 SSE-only 或 baseline
#if defined(USE_SSE)
    return cpu_sum_log_sqrt_sse(data, n);
#else
    return cpu_sum_log_sqrt(data, n);
#endif
#endif
}

inline float cpu_max_log_sqrt_sse_omp(const float* data, uint64_t n) {
#if defined(USE_OPENMP)
    // OpenMP 让多个线程同时计算最大值。因为“最大值”不是简单相加，不能直接用 reduction(max)
    //（不同编译器支持情况不一），所以用“每线程局部最大值 + 临界区合并”的方式。
    float global_max = -INFINITY;

#if defined(USE_SSE)
    const uint64_t n4 = (n / 4) * 4;

#pragma omp parallel
    {
        // OpenMP：每个线程先维护自己的 local_max，避免多个线程同时改 global_max 造成竞态。
        float local_max = -INFINITY;

        // OpenMP：把循环块分给线程；nowait 表示这段循环结束后不用等待其它线程，直接进入后面的合并步骤。
#pragma omp for schedule(static) nowait
        for (long long i = 0; i < (long long)n4; i += 4) {
            __m128 x = _mm_loadu_ps(data + i);
            __m128 r = _mm_sqrt_ps(x);

            alignas(16) float buf[4];
            _mm_store_ps(buf, r);

            // logf 标量，更新 local_max
            for (int k = 0; k < 4; ++k) {
                float v = logf(buf[k]);
                local_max = (v > local_max ? v : local_max);
            }
        }

        // OpenMP：用 critical 把“每个线程的最大值”安全地合并到全局最大值（一次只允许一个线程进入）。
#pragma omp critical
        {
            global_max = (local_max > global_max ? local_max : global_max);
        }
    }

    // 尾巴补算（只算一次）
    // 并行区结束后再串行补尾巴，保证每个元素只算一次。
    for (uint64_t i = n4; i < n; ++i) {
        float v = logf(sqrtf(data[i]));
        global_max = (v > global_max ? v : global_max);
    }
    return global_max;

#else
#pragma omp parallel
    {
        // OpenMP：每个线程算自己的 local_max，最后在 critical 区域合并，避免写冲突。
        float local_max = -INFINITY;

        // OpenMP：分配循环块给线程，nowait 让线程不用在这里等其它线程。
#pragma omp for schedule(static) nowait
        for (long long i = 0; i < (long long)n; ++i) {
            float v = logf(sqrtf(data[i]));
            local_max = (v > local_max ? v : local_max);
        }

        // OpenMP：critical 区域负责合并每个线程的结果，避免并发写冲突。
#pragma omp critical
        {
            global_max = (local_max > global_max ? local_max : global_max);
        }
    }
    return global_max;
#endif

#else
    // 没开 OpenMP：退化为 SSE-only 或 baseline
#if defined(USE_SSE)
    return cpu_max_log_sqrt_sse(data, n);
#else
    return cpu_max_log_sqrt(data, n);
#endif
#endif
}
