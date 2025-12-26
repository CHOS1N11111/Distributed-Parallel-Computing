/**
 * @file cpu_sort.h
 * @brief 排序与归并逻辑模块
 * * 该文件实现了基于 ln(sqrt(x)) 为比较键（Key）的排序算法。
 * 主要包含：
 * 1. key_log_sqrt: 计算比较键。
 * 2. quicksort_by_key: 对原始数据进行原地快速排序，但依据变换后的 Key 进行比较。
 * 3. merge_to_transformed: 将两段已排序的原始数据归并，并直接输出变换后的有序序列。
 * * 用于 Master 的本地排序以及合并 Worker 返回的有序数据。
 */
#pragma once
#include <cmath>
#include <cstdint>

//cpu_sort.h：
//提供按 log(sqrt(x)) 为比较键的排序与归并逻辑，包括 key_log_sqrt、quicksort_by_key 和 merge_to_transformed，用于得到全局有序的 log(sqrt(.)) 序列。

//
static inline float key_log_sqrt(float x) {
    return logf(sqrtf(x));
}

// 原地快速排序，按 key_log_sqrt(x) 升序排列 a[l..r]，流程为：
// 1) 选中间元素为 pivot，并计算其 key， kp = key_log_sqrt(pivot)。
// 2) i/j 双指针向中间扫描，找到与 key 关系不满足的元素。
// 3) 交换 a[i]/a[j] 并推进指针，完成一轮分区。
// 4) 递归处理左右子区间，直到区间长度为 1。
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
