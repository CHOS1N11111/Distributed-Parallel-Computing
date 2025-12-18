# Distributed Log-Sqrt Calculator (Master-Worker Model)

这是一个基于 C++ 和 Windows Socket (Winsock2) 的简单分布式计算演示项目。采用 **Master-Worker** 架构，将计算密集型任务在两台计算机（或两个进程）间分配，并对比单机计算与双机协同计算的性能差异。

## 📄 项目简介

处理大规模浮点数组（默认约 1.28 × 10^8 个元素），对每个元素 `x` 执行 `ln(sqrt(x))` 变换，并支持以下核心操作：
- **求和 (SUM)**：计算变换后数据的总和
- **求最大值 (MAX)**：计算变换后数据的最大值
- **排序 (SORT)**：按变换后的值排序（包含分布式归并）

Master 负责任务拆分、本地计算、收集 Worker 结果并汇总。

## 🛠️ 环境依赖

- **操作系统**：Windows
- **编译器**：支持 C++11 及以上
- **库依赖**：`Ws2_32.lib`（代码通过 `#pragma comment` 引入，通常无需额外配置）

## 📂 文件结构

- **核心逻辑**
  - `master.cpp`: 主节点入口，基准测试、连接 Worker、分发任务与汇总结果
  - `worker.cpp`: 从节点入口，监听端口、接收指令、处理数据并返回结果

- **计算内核**
  - `cpu_ops.h`: SIMD 友好的求和和求最大值实现
  - `cpu_sort.h`: 自定义快速排序与归并排序逻辑，以 `ln(sqrt(x))` 作为比较键

- **网络通信**
  - `net.h` / `net.cpp`: 封装 Winsock 初始化、连接、发送 (`send_all`)、接收 (`recv_all`)
  - `common.h`: 定义通信协议 (`MsgHeader`)、端口、数据规模常量与工具函数

## ⚙️ 配置说明

1. **修改 Worker IP**
   打开 `master.cpp`，找到：
   ```cpp
   const char* WORKER_IP = "192.168.137.5"; // TODO: 替换为你的 Worker IP
   ```
   单机测试可设为 `127.0.0.1`。

2. **端口配置（可选）**
   默认端口为 `50001`（定义于 `common.h`）。若端口被占用，可在 `common.h` 中修改 `PORT`。

## 🚀 编译与运行

推荐使用 **Visual Studio** 创建解决方案，包含两个项目：
1. **Master**：`master.cpp`, `net.cpp`, `net.h` 等
2. **Worker**：`worker.cpp`, `net.cpp`, `net.h` 等

命令行（MSVC 示例）：
```cmd
cl /EHsc /O2 master.cpp net.cpp /Fe:master.exe
cl /EHsc /O2 worker.cpp net.cpp /Fe:worker.exe
```

### 运行步骤
1. **启动 Worker**：先运行 `worker.exe`，应看到 `[Worker] Listening on 50001...`
2. **启动 Master**：再运行 `master.exe`，流程：
   - 运行单机基准 (Base Run)
   - 连接 Worker
   - 运行双机协同 (Dual Run)，将部分数据（默认 70% 可调）交给 Worker
   - 输出耗时对比与结果校验

## 📊 性能测试逻辑

程序默认执行 5 次取平均值 (`RUN5_AVG`) 以获得稳定数据。
- **Data Size**：`DATANUM` 默认 1.28 亿个 float（约 512MB 内存）
- **Sum/Max**：Worker 仅返回 1 个 float，网络开销极小，加速比主要取决于 CPU 计算
- **Sort**：Worker 排序后需回传完整数据给 Master 归并，受带宽影响较大

## ⚠️ 注意事项

- **防火墙**：跨机运行时，确保 Worker 机器的 Windows 防火墙放行端口 `50001` 的入站 TCP 连接
- **数据一致性**：程序使用确定性算法生成数据（`init_local`），Master 与 Worker 无需传输原始数据即可生成一致的测试数据集
