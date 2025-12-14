// net.cpp
#include "net.h"
#include <stdexcept>

WsaInit::WsaInit() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) throw std::runtime_error("WSAStartup failed");
}
WsaInit::~WsaInit() { WSACleanup(); }

static SOCKET mk_socket() {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) throw std::runtime_error("socket failed");
    return s;
}

SOCKET tcp_listen(uint16_t port) {
    SOCKET s = mk_socket();
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) throw std::runtime_error("bind failed");
    if (listen(s, 1) == SOCKET_ERROR) throw std::runtime_error("listen failed");
    return s;
}

SOCKET tcp_accept(SOCKET listenSock) {
    SOCKET c = accept(listenSock, nullptr, nullptr);
    if (c == INVALID_SOCKET) throw std::runtime_error("accept failed");
    return c;
}

SOCKET tcp_connect(const char* ip, uint16_t port) {
    SOCKET s = mk_socket();
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) throw std::runtime_error("inet_pton failed");

    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) throw std::runtime_error("connect failed");
    return s;
}

bool send_all(SOCKET s, const void* data, size_t bytes) {
    const char* p = (const char*)data;
    while (bytes) {
        int n = send(s, p, (int)bytes, 0);
        if (n <= 0) return false;
        p += n; bytes -= (size_t)n;
    }
    return true;
}

bool recv_all(SOCKET s, void* data, size_t bytes) {
    char* p = (char*)data;
    while (bytes) {
        int n = recv(s, p, (int)bytes, 0);
        if (n <= 0) return false;
        p += n; bytes -= (size_t)n;
    }
    return true;
}

void close_sock(SOCKET s) {
    if (s != INVALID_SOCKET) closesocket(s);
}
