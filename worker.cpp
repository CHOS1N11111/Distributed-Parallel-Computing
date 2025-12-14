#include "common.h"
#include "net.h"
#include "cuda_ops.cuh"
#include "cpu_sort.h"
#include <vector>
#include <iostream>

static void init_local(std::vector<float>& data, uint64_t begin, uint64_t end) {
    uint64_t n = end - begin;
    data.resize((size_t)n);
    for (uint64_t i = 0; i < n; ++i) {
        data[(size_t)i] = (float)(begin + i + 1); // i+1，且 begin 控制值域不重叠
    }
}

int main() {
    WsaInit wsa;
    SOCKET ls = tcp_listen(PORT);
    std::cout << "[Worker] Listening on " << PORT << "...\n";
    SOCKET c = tcp_accept(ls);
    std::cout << "[Worker] Connected.\n";

    while (true) {
        MsgHeader h{};
        if (!recv_all(c, &h, sizeof(h))) break;
        if (h.magic != MAGIC) break;

        std::vector<float> local;
        init_local(local, h.begin, h.end);
        int64_t n = (int64_t)local.size();

        if (h.op == (uint32_t)Op::SUM) {
            float part = cuda_sum_log_sqrt(local.data(), n);
            send_all(c, &part, sizeof(part));
        }
        else if (h.op == (uint32_t)Op::MAX) {
            float part = cuda_max_log_sqrt(local.data(), n);
            send_all(c, &part, sizeof(part));
        }
        else if (h.op == (uint32_t)Op::SORT) {
            // 本地按 key 排序（原始值数组）
            quicksort_by_key(local.data(), 0, n - 1);

            // 为了降低 master 端重复算，可以直接把“原始值排序结果”发回去
            // master 合并时再实时计算 key 输出最终 result
            uint64_t bytes = (uint64_t)n * sizeof(float);
            send_all(c, &bytes, sizeof(bytes));
            send_all(c, local.data(), (size_t)bytes);
        }
        else {
            break;
        }
    }

    close_sock(c);
    close_sock(ls);
    return 0;
}
