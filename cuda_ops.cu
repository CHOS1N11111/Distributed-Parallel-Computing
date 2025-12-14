// cuda_ops.cu
#include "cuda_ops.cuh"
#include <cuda_runtime.h>
#include <cmath>
#include <vector>
#include <stdexcept>

static inline void ck(cudaError_t e) {
    if (e != cudaSuccess) throw std::runtime_error(cudaGetErrorString(e));
}

__device__ __forceinline__ float f_log_sqrt(float x) {
    return logf(sqrtf(x));
}

__global__ void sum_kernel(const float* a, int64_t n, float* blockOut) {
    extern __shared__ float sdata[];
    int tid = threadIdx.x;
    int64_t i = (int64_t)blockIdx.x * (blockDim.x * 2LL) + threadIdx.x;

    float acc = 0.0f;
    if (i < n) acc += f_log_sqrt(a[i]);
    if (i + blockDim.x < n) acc += f_log_sqrt(a[i + blockDim.x]);

    sdata[tid] = acc;
    __syncthreads();

    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) sdata[tid] += sdata[tid + s];
        __syncthreads();
    }
    if (tid == 0) blockOut[blockIdx.x] = sdata[0];
}

__global__ void max_kernel(const float* a, int64_t n, float* blockOut) {
    extern __shared__ float sdata[];
    int tid = threadIdx.x;
    int64_t i = (int64_t)blockIdx.x * (blockDim.x * 2LL) + threadIdx.x;

    float m = -INFINITY;
    if (i < n) m = fmaxf(m, f_log_sqrt(a[i]));
    if (i + blockDim.x < n) m = fmaxf(m, f_log_sqrt(a[i + blockDim.x]));

    sdata[tid] = m;
    __syncthreads();

    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) sdata[tid] = fmaxf(sdata[tid], sdata[tid + s]);
        __syncthreads();
    }
    if (tid == 0) blockOut[blockIdx.x] = sdata[0];
}

static float reduce_sum_on_gpu(const float* d_in, int64_t n) {
    const int block = 256;
    int grid = (int)((n + block * 2LL - 1) / (block * 2LL));

    float* d_block = nullptr;
    ck(cudaMalloc(&d_block, grid * sizeof(float)));

    sum_kernel<<<grid, block, block * sizeof(float)>>>(d_in, n, d_block);
    ck(cudaGetLastError());

    std::vector<float> h_block(grid);
    ck(cudaMemcpy(h_block.data(), d_block, grid * sizeof(float), cudaMemcpyDeviceToHost));
    ck(cudaFree(d_block));

    double sum = 0.0;
    for (int i = 0; i < grid; ++i) sum += h_block[i];
    return (float)sum;
}

static float reduce_max_on_gpu(const float* d_in, int64_t n) {
    const int block = 256;
    int grid = (int)((n + block * 2LL - 1) / (block * 2LL));

    float* d_block = nullptr;
    ck(cudaMalloc(&d_block, grid * sizeof(float)));

    max_kernel<<<grid, block, block * sizeof(float)>>>(d_in, n, d_block);
    ck(cudaGetLastError());

    std::vector<float> h_block(grid);
    ck(cudaMemcpy(h_block.data(), d_block, grid * sizeof(float), cudaMemcpyDeviceToHost));
    ck(cudaFree(d_block));

    float m = -INFINITY;
    for (int i = 0; i < grid; ++i) m = (h_block[i] > m ? h_block[i] : m);
    return m;
}

float cuda_sum_log_sqrt(const float* hostData, int64_t n) {
    float* d = nullptr;
    ck(cudaMalloc(&d, n * sizeof(float)));
    ck(cudaMemcpy(d, hostData, n * sizeof(float), cudaMemcpyHostToDevice));
    float r = reduce_sum_on_gpu(d, n);
    ck(cudaFree(d));
    return r;
}

float cuda_max_log_sqrt(const float* hostData, int64_t n) {
    float* d = nullptr;
    ck(cudaMalloc(&d, n * sizeof(float)));
    ck(cudaMemcpy(d, hostData, n * sizeof(float), cudaMemcpyHostToDevice));
    float r = reduce_max_on_gpu(d, n);
    ck(cudaFree(d));
    return r;
}
