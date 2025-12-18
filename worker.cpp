#include "common.h"
#include "net.h"
#include "cpu_ops.h"
#include "cpu_sort.h"
#include <vector>
#include <iostream>

#include <exception>
/**
 * @file worker.cpp
 * @brief 从节点 (Worker) 入口程序
 * * 程序的计算服务中心。主要职责包括：
 * 1. 作为 TCP 服务端监听指定端口，等待 Master 连接。
 * 2. 消息循环：接收 Master 发送的操作指令 (MsgHeader)。
 * 3. 数据生成：根据指令中的范围 (begin, end) 自行生成数据，避免网络传输原始数据。
 * 4. 任务执行：执行对应的 sum/max/sort 计算。
 * 5. 结果回传：将计算结果（数值或排序后的数组）发送回 Master。
 */


// 生成 [begin, end) 的递增数据（元素值为 begin+1 起步），便于 master/worker 双端保持一致
static void init_local(std::vector<float>& data, uint64_t begin, uint64_t end) {
    uint64_t n = end - begin;
    data.resize((size_t)n);
    for (uint64_t i = 0; i < n; ++i) {
        data[(size_t)i] = (float)(begin + i + 1);
    }
}

int main() {
    try {
        // 初始化 WSA 并启动监听，等待 master 连接
        // worker 只接受一个连接，主循环内串行处理 master 发来的多轮指令
        WsaInit wsa;
        std::cout << "[Worker] BOOT OK\n";

        SOCKET ls = tcp_listen(PORT);
        std::cout << "[Worker] Listening on " << PORT << "...\n";
        SOCKET c = tcp_accept(ls);
        std::cout << "[Worker] Connected.\n";

        // 循环处理来自 master 的任务
        while (true) {
            std::cout << "[Worker] waiting header...\n";
            MsgHeader h{};
            // 先收头部，失败即退出；此时尚未收正文，安全返回等待下一次连接
            if (!recv_all(c, &h, sizeof(h))) {
                std::cout << "[Worker] header: magic=" << std::hex << h.magic
                    << " op=" << std::dec << h.op
                    << " begin=" << h.begin
                    << " end=" << h.end
                    << " len=" << h.len << "\n";
                break;
            }
            std::cout << "[Worker] got header\n";
            std::cout << "[Worker] header: magic=0x" << std::hex << h.magic
                << " op=" << std::dec << h.op
                << " begin=" << h.begin
                << " end=" << h.end
                << " len=" << h.len << "\n";


            // 基础校验，防止非法请求
            if (h.magic != MAGIC) {
                std::cerr << "[Worker] bad magic\n";
                break;
            }
            if (h.op != (uint32_t)Op::SUM && h.op != (uint32_t)Op::MAX && h.op != (uint32_t)Op::SORT) {
                std::cerr << "[Worker] bad op\n";
                break;
            }
            if (h.end <= h.begin) {
                std::cerr << "[Worker] bad range\n";
                break;
            }

            // 按 master 下发的范围生成数据，所有数据均由 worker 自行生成，不依赖网络传输数据块
            std::vector<float> local;
            std::cout << "[Worker] init_local...\n";
            init_local(local, h.begin, h.end);
            std::cout << "[Worker] init_local done, n=" << local.size() << "\n";
            if (h.op == (uint32_t)Op::SUM) {
                std::cout << "[Worker] cpu sum...\n";
                // 本段数据执行 log(sqrt(x)) 后求和，结果发回 master 进行汇总
                float part = cpu_sum_log_sqrt(local.data(), (uint64_t)local.size());
                std::cout << "[Worker] cpu sum done\n";
                send_all(c, &part, sizeof(part));
                std::cout << "[Worker] send sum done\n";
            }
            else if (h.op == (uint32_t)Op::MAX) {

                std::cout << "[Worker] cpu max...\n";
                // 本段数据执行 log(sqrt(x)) 后取最大，同步给 master
                float part = cpu_max_log_sqrt(local.data(), (uint64_t)local.size());
                std::cout << "[Worker] cpu max done\n";
                send_all(c, &part, sizeof(part));
                std::cout << "[Worker] send max done\n";
            }
            else if (h.op == (uint32_t)Op::SORT) {
                std::cout << "[Worker] sort...\n";
                shuffle_fisher_yates(local.data(), (uint64_t)local.size(),
                    0xBADC0FFEEULL ^ h.begin); // 使用 begin 参与 seed，保证段间差异
                quicksort_by_key(local.data(), 0, (int64_t)local.size() - 1);
                std::cout << "[Worker] local[0]=" << local.front()
                    << " key=" << key_log_sqrt(local.front()) << "\n";
                std::cout << "[Worker] local[last]=" << local.back()
                    << " key=" << key_log_sqrt(local.back()) << "\n";

                std::cout << "[Worker] sort done\n";
                uint64_t bytes = (uint64_t)local.size() * sizeof(float);
                send_all(c, &bytes, sizeof(bytes));
                send_all(c, local.data(), (size_t)bytes);
                std::cout << "[Worker] send sort done\n";
            }
        }

        close_sock(c);
        close_sock(ls);
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "[FATAL Worker] " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "[FATAL Worker] unknown exception" << std::endl;
        return 1;
    }

    system("pause");

}
