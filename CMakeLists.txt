cmake_minimum_required(VERSION 3.24)

project(Project LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# 你的显卡 RTX 4060 是 Ada，一般用 89；如果你不确定，也可以先写 86/89 都行
# 4060 通常 compute capability = 8.9
set(CMAKE_CUDA_ARCHITECTURES 89)

find_package(CUDAToolkit REQUIRED)

# 公共代码做成一个静态库，System/Worker 都链接它
add_library(core STATIC
    net.cpp
    cuda_ops.cu
)

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Winsock + CUDA Runtime
target_link_libraries(core PUBLIC ws2_32 CUDA::cudart)

# Windows 下避免 min/max 宏污染（可选）
target_compile_definitions(core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)

# CUDA 编译属性（可选但推荐）
set_target_properties(core PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
)

# System (master)
add_executable(System
    master.cpp
)
target_link_libraries(System PRIVATE core)

# Worker
add_executable(Worker
    worker.cpp
)
target_link_libraries(Worker PRIVATE core)
