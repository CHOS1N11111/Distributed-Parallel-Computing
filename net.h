
// net.h
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <string>

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
