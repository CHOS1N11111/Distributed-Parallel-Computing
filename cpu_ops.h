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

// ===== SSE/AVX2 (SIMD) support =====
// AVX2 processes 8 floats per step, SSE processes 4; logf stays scalar.

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
// 说明：逐个元素先开方再取对数，并把结果累加到双精度变量中以减少精度损失。
inline float cpu_sum_log_sqrt(const float* data, uint64_t n) {
    double s = 0.0;
    for (uint64_t i = 0; i < n; ++i) {
        // 第一步：对当前元素开方
        // 第二步：对开方结果取对数
        // 第三步：把该结果累加到总和
        s += logf(sqrtf(data[i]));
    }
    // 把双精度的累加结果转换为 float 返回
    return (float)s;
}

// 计算 log(sqrt(x)) 的最大值
// 说明：逐个元素先开方再取对数，与当前最大值比较并更新。
inline float cpu_max_log_sqrt(const float* data, uint64_t n) {
    // 初始值设为负无穷，确保第一个元素一定能更新最大值
    float m = -INFINITY;
    for (uint64_t i = 0; i < n; ++i) {
        // 计算当前元素的 log(sqrt(x))
        float v = logf(sqrtf(data[i]));
        // 如果当前值更大则更新最大值
        m = (v > m ? v : m);
    }
    // 返回最终最大值
    return m;
}

// ===== SSE/AVX2 version (for SpeedUp path) =====
// 说明：sqrt 使用 SIMD（AVX2 每次 8 个元素或 SSE 每次 4 个元素）并行计算，
// logf 仍保持标量逐个计算，以保证与基线版本的数值一致性。
inline float cpu_sum_log_sqrt_sse(const float* data, uint64_t n) {
#if defined(USE_SSE) && defined(USE_AVX2)
    double s = 0.0;
    uint64_t i = 0;
    alignas(32) float buf[8];

    for (; i + 8 <= n; i += 8) {
        // 从内存加载 8 个 float 到 AVX 寄存器
        __m256 x = _mm256_loadu_ps(data + i);
        // 对 8 个元素并行开方
        __m256 r = _mm256_sqrt_ps(x);
        // 将结果存回对齐缓冲区，便于标量 logf 处理
        _mm256_store_ps(buf, r);
        // 逐个计算 logf 并累加
        s += logf(buf[0]) + logf(buf[1]) + logf(buf[2]) + logf(buf[3]) +
             logf(buf[4]) + logf(buf[5]) + logf(buf[6]) + logf(buf[7]);
    }
    // 处理剩余不足 8 个的尾部元素
    for (; i < n; ++i) s += logf(sqrtf(data[i]));
    return (float)s;
#elif defined(USE_SSE)
    double s = 0.0;
    uint64_t i = 0;
    alignas(16) float buf[4];

    for (; i + 4 <= n; i += 4) {
        // 从内存加载 4 个 float 到 SSE 寄存器
        __m128 x = _mm_loadu_ps(data + i);
        // 对 4 个元素并行开方
        __m128 r = _mm_sqrt_ps(x);
        // 将结果存回对齐缓冲区，便于标量 logf 处理
        _mm_store_ps(buf, r);
        // 逐个计算 logf 并累加
        s += logf(buf[0]) + logf(buf[1]) + logf(buf[2]) + logf(buf[3]);
    }
    // 处理剩余不足 4 个的尾部元素
    for (; i < n; ++i) s += logf(sqrtf(data[i]));
    return (float)s;
#else
    return cpu_sum_log_sqrt(data, n);
#endif
}

inline float cpu_max_log_sqrt_sse(const float* data, uint64_t n) {
#if defined(USE_SSE) && defined(USE_AVX2)
    float m = -INFINITY;
    uint64_t i = 0;
    alignas(32) float buf[8];

    for (; i + 8 <= n; i += 8) {
        // 从内存加载 8 个 float 到 AVX 寄存器
        __m256 x = _mm256_loadu_ps(data + i);
        // 对 8 个元素并行开方
        __m256 r = _mm256_sqrt_ps(x);
        // 将结果存回对齐缓冲区，便于标量 logf 处理
        _mm256_store_ps(buf, r);
        for (int k = 0; k < 8; ++k) {
            // 逐个计算 logf，并更新局部最大值
            float v = logf(buf[k]);
            m = (v > m ? v : m);
        }
    }
    // 处理剩余不足 8 个的尾部元素
    for (; i < n; ++i) {
        float v = logf(sqrtf(data[i]));
        m = (v > m ? v : m);
    }
    return m;
#elif defined(USE_SSE)
    float m = -INFINITY;
    uint64_t i = 0;
    alignas(16) float buf[4];

    for (; i + 4 <= n; i += 4) {
        // 从内存加载 4 个 float 到 SSE 寄存器
        __m128 x = _mm_loadu_ps(data + i);
        // 对 4 个元素并行开方
        __m128 r = _mm_sqrt_ps(x);
        // 将结果存回对齐缓冲区，便于标量 logf 处理
        _mm_store_ps(buf, r);
        for (int k = 0; k < 4; ++k) {
            // 逐个计算 logf，并更新局部最大值
            float v = logf(buf[k]);
            m = (v > m ? v : m);
        }
    }
    // 处理剩余不足 4 个的尾部元素
    for (; i < n; ++i) {
        float v = logf(sqrtf(data[i]));
        m = (v > m ? v : m);
    }
    return m;
#else
    return cpu_max_log_sqrt(data, n);
#endif
}

// -------------------- OpenMP + SIMD (same function uses both) --------------------
// 设计说明：
// - OpenMP：把外层循环按块分配给多个线程并行执行。
// - SIMD：每个线程内部用 AVX2/SSE 对元素并行开方。
// - logf：仍使用标量逐个计算，保证与基线结果一致。
// - 尾部处理：当 n 不是 8/4 的整数倍时，剩余元素使用标量串行处理。

inline float cpu_sum_log_sqrt_sse_omp(const float* data, uint64_t n) {
#if defined(USE_OPENMP)
    double sum = 0.0;

#if defined(USE_SSE) && defined(USE_AVX2)
    const uint64_t n8 = (n / 8) * 8;
#pragma omp parallel for reduction(+:sum) schedule(static)
    for (long long i = 0; i < (long long)n8; i += 8) {
        // 并行线程内：加载 8 个元素并行开方
        __m256 x = _mm256_loadu_ps(data + i);
        __m256 r = _mm256_sqrt_ps(x);

        alignas(32) float buf[8];
        // 将开方结果写回缓冲，便于标量 logf
        _mm256_store_ps(buf, r);

        // 每个线程累加本地的 logf 结果，OpenMP 负责归约到 sum
        sum += logf(buf[0]) + logf(buf[1]) + logf(buf[2]) + logf(buf[3]) +
               logf(buf[4]) + logf(buf[5]) + logf(buf[6]) + logf(buf[7]);
    }

    // 处理尾部元素（不足 8 个的部分）
    for (uint64_t i = n8; i < n; ++i) sum += logf(sqrtf(data[i]));
    return (float)sum;

#elif defined(USE_SSE)
    const uint64_t n4 = (n / 4) * 4;
#pragma omp parallel for reduction(+:sum) schedule(static)
    for (long long i = 0; i < (long long)n4; i += 4) {
        // 并行线程内：加载 4 个元素并行开方
        __m128 x = _mm_loadu_ps(data + i);
        __m128 r = _mm_sqrt_ps(x);

        alignas(16) float buf[4];
        // 将开方结果写回缓冲，便于标量 logf
        _mm_store_ps(buf, r);

        // 每个线程累加本地的 logf 结果，OpenMP 负责归约到 sum
        sum += logf(buf[0]) + logf(buf[1]) + logf(buf[2]) + logf(buf[3]);
    }

    // 处理尾部元素（不足 4 个的部分）
    for (uint64_t i = n4; i < n; ++i) sum += logf(sqrtf(data[i]));
    return (float)sum;

#else
#pragma omp parallel for reduction(+:sum) schedule(static)
    for (long long i = 0; i < (long long)n; ++i) {
        // 没有 SIMD 时，直接标量计算并由 OpenMP 归约
        sum += logf(sqrtf(data[i]));
    }
    return (float)sum;
#endif

#else
#if defined(USE_SSE)
    return cpu_sum_log_sqrt_sse(data, n);
#else
    return cpu_sum_log_sqrt(data, n);
#endif
#endif
}

inline float cpu_max_log_sqrt_sse_omp(const float* data, uint64_t n) {
#if defined(USE_OPENMP)
    float global_max = -INFINITY;

#if defined(USE_SSE) && defined(USE_AVX2)
    const uint64_t n8 = (n / 8) * 8;

#pragma omp parallel
    {
        // 每个线程维护自己的局部最大值，避免频繁竞争
        float local_max = -INFINITY;

#pragma omp for schedule(static) nowait
        for (long long i = 0; i < (long long)n8; i += 8) {
            // 并行线程内：加载 8 个元素并行开方
            __m256 x = _mm256_loadu_ps(data + i);
            __m256 r = _mm256_sqrt_ps(x);

            alignas(32) float buf[8];
            // 将开方结果写回缓冲，便于标量 logf
            _mm256_store_ps(buf, r);

            for (int k = 0; k < 8; ++k) {
                // 逐个计算 logf，并更新线程内最大值
                float v = logf(buf[k]);
                local_max = (v > local_max ? v : local_max);
            }
        }

#pragma omp critical
        {
            // 合并线程内最大值到全局最大值
            global_max = (local_max > global_max ? local_max : global_max);
        }
    }

    // 处理尾部元素（不足 8 个的部分），直接更新全局最大值
    for (uint64_t i = n8; i < n; ++i) {
        float v = logf(sqrtf(data[i]));
        global_max = (v > global_max ? v : global_max);
    }
    return global_max;

#elif defined(USE_SSE)
    const uint64_t n4 = (n / 4) * 4;

#pragma omp parallel
    {
        // 每个线程维护自己的局部最大值，避免频繁竞争
        float local_max = -INFINITY;

#pragma omp for schedule(static) nowait
        for (long long i = 0; i < (long long)n4; i += 4) {
            // 并行线程内：加载 4 个元素并行开方
            __m128 x = _mm_loadu_ps(data + i);
            __m128 r = _mm_sqrt_ps(x);

            alignas(16) float buf[4];
            // 将开方结果写回缓冲，便于标量 logf
            _mm_store_ps(buf, r);

            for (int k = 0; k < 4; ++k) {
                // 逐个计算 logf，并更新线程内最大值
                float v = logf(buf[k]);
                local_max = (v > local_max ? v : local_max);
            }
        }

#pragma omp critical
        {
            // 合并线程内最大值到全局最大值
            global_max = (local_max > global_max ? local_max : global_max);
        }
    }

    // 处理尾部元素（不足 4 个的部分），直接更新全局最大值
    for (uint64_t i = n4; i < n; ++i) {
        float v = logf(sqrtf(data[i]));
        global_max = (v > global_max ? v : global_max);
    }
    return global_max;

#else
#pragma omp parallel
    {
        // 每个线程维护自己的局部最大值，避免频繁竞争
        float local_max = -INFINITY;

#pragma omp for schedule(static) nowait
        for (long long i = 0; i < (long long)n; ++i) {
            // 直接标量计算并更新线程内最大值
            float v = logf(sqrtf(data[i]));
            local_max = (v > local_max ? v : local_max);
        }

#pragma omp critical
        {
            // 合并线程内最大值到全局最大值
            global_max = (local_max > global_max ? local_max : global_max);
        }
    }
    return global_max;
#endif

#else
#if defined(USE_SSE)
    return cpu_max_log_sqrt_sse(data, n);
#else
    return cpu_max_log_sqrt(data, n);
#endif
#endif
}
