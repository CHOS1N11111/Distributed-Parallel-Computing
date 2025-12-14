// cuda_ops.cuh
#pragma once
#include <cstdint>

float cuda_sum_log_sqrt(const float* hostData, int64_t n);
float cuda_max_log_sqrt(const float* hostData, int64_t n);
