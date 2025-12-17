#include "common.h"
#include "net.h"
#include "cpu_ops.h"
#include "cpu_sort.h"
#include <vector>
#include <iostream>

#include <exception>


static void init_local(std::vector<float>& data, uint64_t begin, uint64_t end) {
    uint64_t n = end - begin;
    data.resize((size_t)n);
    for (uint64_t i = 0; i < n; ++i) {
        data[(size_t)i] = (float)(begin + i + 1); // i+1���� begin ����ֵ���ص�
    }
}

int main() {
    try {
        WsaInit wsa;
        std::cout << "[Worker] BOOT OK\n";

        SOCKET ls = tcp_listen(PORT);
        std::cout << "[Worker] Listening on " << PORT << "...\n";
        SOCKET c = tcp_accept(ls);
        std::cout << "[Worker] Connected.\n";

        while (true) {
            std::cout << "[Worker] waiting header...\n";
            MsgHeader h{};
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


            std::vector<float> local;
            std::cout << "[Worker] init_local...\n";
            init_local(local, h.begin, h.end);
            std::cout << "[Worker] init_local done, n=" << local.size() << "\n";

            if (h.op == (uint32_t)Op::SUM) {
                std::cout << "[Worker] cpu sum...\n";
                float part = cpu_sum_log_sqrt(local.data(), (uint64_t)local.size());
                std::cout << "[Worker] cpu sum done\n";
                send_all(c, &part, sizeof(part));
                std::cout << "[Worker] send sum done\n";
            }
            else if (h.op == (uint32_t)Op::MAX) {
                std::cout << "[Worker] cpu max...\n";

                float part = cpu_max_log_sqrt(local.data(), (uint64_t)local.size());
                std::cout << "[Worker] cpu max done\n";
                send_all(c, &part, sizeof(part));
                std::cout << "[Worker] send max done\n";
            }
            else if (h.op == (uint32_t)Op::SORT) {
                std::cout << "[Worker] sort...\n";
                shuffle_fisher_yates(local.data(), (uint64_t)local.size(),
                    0xBADC0FFEEULL ^ h.begin); // seed 可用 begin 搅一下，保证每段不同
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