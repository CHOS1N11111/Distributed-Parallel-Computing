#
cmake_minimum_required(VERSION 3.16)

project(AlgoBigHW LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- library: net ----
add_library(net STATIC
    net.cpp
    net.h
)
target_include_directories(net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Windows socket / headers cleanup
if(WIN32)
    target_compile_definitions(net PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(net PUBLIC ws2_32)
endif()

# (Optional but safe) threads for future use
find_package(Threads REQUIRED)

# ---- executables ----
add_executable(master
    master.cpp
    common.h
    cpu_ops.h
    cpu_sort.h
)
target_link_libraries(master PRIVATE net Threads::Threads)
set_target_properties(master PROPERTIES OUTPUT_NAME "Master")

add_executable(worker
    worker.cpp
    common.h
    cpu_ops.h
    cpu_sort.h
)
target_link_libraries(worker PRIVATE net Threads::Threads)
set_target_properties(worker PROPERTIES OUTPUT_NAME "Worker")

option(USE_SSE "Enable SSE/SSE2 intrinsics" ON) #TODO： SSE选项，默认开启，如需关闭，请改为 OFF 或在 CMake 命令行使用 -DUSE_SSE=OFF。

if(USE_SSE)

  target_compile_definitions(master PRIVATE USE_SSE=1)
  target_compile_definitions(worker PRIVATE USE_SSE=1)

  if(MSVC AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_options(master PRIVATE /arch:SSE2)
    target_compile_options(worker PRIVATE /arch:SSE2)
  elseif(NOT MSVC)
    target_compile_options(master PRIVATE -msse2)
    target_compile_options(worker PRIVATE -msse2)
  endif()
endif()

option(USE_OPENMP "Enable OpenMP" ON)  #TODO： OpenMP选项，如需关闭，请改为 OFF 或在 CMake 命令行使用 -DUSE_OPENMP=OFF

if(USE_OPENMP)
  find_package(OpenMP)
  if(OpenMP_CXX_FOUND)

    target_link_libraries(master PRIVATE OpenMP::OpenMP_CXX)
    target_link_libraries(worker PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(master PRIVATE USE_OPENMP=1)
    target_compile_definitions(worker PRIVATE USE_OPENMP=1)
  else()
    message(WARNING "OpenMP not found; building without OpenMP.")
  endif()
endif()
