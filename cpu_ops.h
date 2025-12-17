#pragma once
#include <cmath>
#include <cstdint>

//cpu_ops.h：
//提供对原始 float 数据逐个计算 log(sqrt(x)) 后求和/求最大值的 CPU 实现（cpu_sum_log_sqrt、cpu_max_log_sqrt），不涉及排序。

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
