\# Distributed Log-Sqrt Calculator (Master-Worker Model)



这是一个基于 C++ 和 Windows Socket (Winsock2) 实现的简易分布式计算演示项目。它采用了 \*\*Master-Worker\*\* 架构，将计算密集型任务在两台计算机（或两个进程）之间进行分配，并对比单机计算与双机协同计算的性能差异。



\## 📝 项目简介



本项目旨在处理大规模浮点数数组（默认为 1.28 \\times 10^8 个元素），对每个元素 x 执行 \\ln(\\sqrt{x}) 变换，并支持以下三种核心操作：



1\. \*\*求和 (SUM)\*\*：计算变换后数据的总和。

2\. \*\*求最大值 (MAX)\*\*：寻找变换后数据的最大值。

3\. \*\*排序 (SORT)\*\*：基于变换后的值对数据进行排序（包含分布式归并）。



Master 节点负责任务分发、本地计算部分数据、收集 Worker 结果以及最终的汇总/归并。



\## 🛠️ 环境依赖



\* \*\*操作系统\*\*: Windows

\* \*\*编译器\*\*: 支持 C++11 或更高版本的编译器

\* \*\*库依赖\*\*: `Ws2\_32.lib` (代码中已通过 `#pragma comment` 包含，通常无需额外配置)



\## 📂 文件结构



\* \*\*核心逻辑\*\*:

\* `master.cpp`: 主节点程序入口。负责基准测试、连接 Worker、分发任务和汇总结果。

\* `worker.cpp`: 从节点程序入口。作为服务端监听端口，接收指令，处理数据并返回结果。





\* \*\*计算内核\*\*:

\* `cpu\_ops.h`: 包含 SIMD 友好的求和与求最大值的基础 CPU 实现。

\* `cpu\_sort.h`: 包含自定义的快速排序和归并排序逻辑，以 \\ln(\\sqrt{x}) 为比较键。





\* \*\*网络通信\*\*:

\* `net.h` / `net.cpp`: 封装 Winsock 的初始化、连接、发送 (`send\_all`) 和接收 (`recv\_all`) 接口。

\* `common.h`: 定义通信协议 (`MsgHeader`)、端口号、数据规模常数及工具函数。







\## ⚙️ 配置说明



1\. \*\*修改 Worker IP 地址\*\*:

打开 `master.cpp`，找到如下代码行，将其修改为运行 `worker` 程序的机器的实际 IP 地址（如果是单机测试，可填 `127.0.0.1`）：

```cpp

// master.cpp

const char\* WORKER\_IP = "192.168.137.5"; // TODO: 修改为你的 Worker IP



```





2\. \*\*端口配置 (可选)\*\*:

默认端口为 `50001`，定义在 `common.h` 中。如果该端口被占用，请在 `common.h` 中修改 `PORT` 常量。



\## 🚀 编译与运行



建议使用 \*\*Visual Studio\*\* 创建一个解决方案，包含两个项目（Project）：



1\. \*\*Master\*\*: 包含 `master.cpp`, `net.cpp`, `net.h` 等。

2\. \*\*Worker\*\*: 包含 `worker.cpp`, `net.cpp`, `net.h` 等。



或者使用命令行编译（以 MSVC 为例）：



```cmd

cl /EHsc /O2 master.cpp net.cpp /Fe:master.exe

cl /EHsc /O2 worker.cpp net.cpp /Fe:worker.exe



```



\### 运行步骤



1\. \*\*启动 Worker\*\*:

先运行 `worker.exe`。它会启动 TCP 监听并显示 `\[Worker] Listening on 50001...`。

2\. \*\*启动 Master\*\*:

再运行 `master.exe`。它将执行以下流程：

\* 运行单机基准测试 (Base Run)。

\* 连接 Worker 节点。

\* 运行双机协同测试 (Dual Run)，将一半（可选 70% ）的数据交给 Worker 处理。

\* 输出两者的时间消耗对比及结果校验。







\## 📊 性能测试逻辑



程序会自动进行 5 次运行取平均值 (`RUN5\_AVG`) 以获得更稳定的性能数据。



\* \*\*Data Size\*\*: 默认 `DATANUM` 为 1.28 亿个 float (约 512MB 内存)。

\* \*\*Sum/Max\*\*: Worker 仅需返回一个 float 结果，网络开销极小，加速比主要取决于 CPU 计算。

\* \*\*Sort\*\*: Worker 完成排序后需将完整数据传回 Master 进行归并，受网络带宽影响较大。



\## ⚠️ 注意事项



\* \*\*防火墙\*\*: 如果在两台不同的机器上运行，请确保 Worker 机器的 Windows 防火墙允许 `50001` 端口的入站 TCP 连接。

\* \*\*数据一致性\*\*: 程序内部使用确定性的算法生成数据 (`init\_local`)，因此 Master 和 Worker 无需传输原始数据即可生成完全一致的测试数据集。

