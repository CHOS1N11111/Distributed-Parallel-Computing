#pragma once
#include <cmath>
#include <cstdint>

// CPU-only implementations for log(sqrt(.)) ops.
// NOTE: input data[] is the *raw* float array. These functions compute log(sqrt(x)) on the fly.

inline float cpu_sum_log_sqrt(const float* data, uint64_t n) {
    double s = 0.0;
    for (uint64_t i = 0; i < n; ++i) {
        s += logf(sqrtf(data[i]));
    }
    return (float)s;
}

inline float cpu_max_log_sqrt(const float* data, uint64_t n) {
    float m = -INFINITY;
    for (uint64_t i = 0; i < n; ++i) {
        float v = logf(sqrtf(data[i]));
        m = (v > m ? v : m);
    }
    return m;
}
