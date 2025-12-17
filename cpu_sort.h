#pragma once
#include <cmath>
#include <cstdint>

//cpu_sort.h：
//提供按 log(sqrt(x)) 为比较键的排序与归并逻辑，包括 key_log_sqrt、quicksort_by_key 和 merge_to_transformed，用于得到全局有序的 log(sqrt(.)) 序列。

//
static inline float key_log_sqrt(float x) {
    return logf(sqrtf(x));
}

// 原地快速排序，按 key_log_sqrt 升序排列原始值
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
// 这样 master 最终得到全局排序后的 log(sqrt(.)) 序列
// 归并两段按 key 排序的原始值，输出转换后的升序序列
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
