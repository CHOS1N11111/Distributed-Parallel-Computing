#pragma once
#include <cmath>
#include <cstdint>

static inline float key_log_sqrt(float x) {
    return logf(sqrtf(x));
}

static void quicksort_by_key(float* a, int64_t l, int64_t r) {
    int64_t i = l, j = r;
    float pivot = a[(l + r) >> 1];
    float kp = key_log_sqrt(pivot);

    while (i <= j) {
        while (key_log_sqrt(a[i]) < kp) ++i;
        while (key_log_sqrt(a[j]) > kp) --j;
        if (i <= j) {
            float t = a[i]; a[i] = a[j]; a[j] = t;
            ++i; --j;
        }
    }
    if (l < j) quicksort_by_key(a, l, j);
    if (i < r) quicksort_by_key(a, i, r);
}

// merge：输入两段已按 key 排序的原始值数组，输出 result 为“变换后值”
// 这样 master 最终得到全局排序后的 log(sqrt(.)) 序列（最符合题意）
static void merge_to_transformed(
    const float* a, int64_t na,
    const float* b, int64_t nb,
    float* outTransformed
) {
    int64_t i = 0, j = 0, k = 0;
    while (i < na && j < nb) {
        float ka = key_log_sqrt(a[i]);
        float kb = key_log_sqrt(b[j]);
        if (ka <= kb) { outTransformed[k++] = ka; ++i; }
        else { outTransformed[k++] = kb; ++j; }
    }
    while (i < na) outTransformed[k++] = key_log_sqrt(a[i++]);
    while (j < nb) outTransformed[k++] = key_log_sqrt(b[j++]);
}
