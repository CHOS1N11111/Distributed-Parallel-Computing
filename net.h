/**
 * @file net.h
 * @brief 网络通信库头文件
 * * 声明了基于 Windows Socket (Winsock2) 的 TCP 网络通信接口。
 * 提供了 Socket 的初始化 (WsaInit)、监听、连接、以及确保数据完整性的
 * 发送 (send_all) 和接收 (recv_all) 函数封装。
 */

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>

struct WsaInit {
    WsaInit();
    ~WsaInit();
};

SOCKET tcp_listen(uint16_t port);
SOCKET tcp_accept(SOCKET listenSock);
SOCKET tcp_connect(const char* ip, uint16_t port);

bool send_all(SOCKET s, const void* data, size_t bytes);
bool recv_all(SOCKET s, void* data, size_t bytes);

void close_sock(SOCKET s);
