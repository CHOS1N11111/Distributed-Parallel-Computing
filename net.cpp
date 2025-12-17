// net.cpp

#pragma comment(lib, "Ws2_32.lib")

#include "net.h"
#include <stdexcept>

// 初始化/清理 WinSock2
WsaInit::WsaInit() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) throw std::runtime_error("WSAStartup failed");
}
WsaInit::~WsaInit() { WSACleanup(); }

// 创建基础 TCP 套接字
static SOCKET mk_socket() {
    // 创建 TCP 流式套接字
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) throw std::runtime_error("socket failed");
    return s;
}

// net.cpp
#include <sstream>

static void throw_wsa(const char* msg) {
    int e = WSAGetLastError();
    std::ostringstream oss;
    oss << msg << " (WSA=" << e << ")";
    throw std::runtime_error(oss.str());
}


// 启动监听（返回监听套接字）
SOCKET tcp_listen(uint16_t port) {
    SOCKET s = mk_socket();
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) throw_wsa("bind failed");
    if (listen(s, 1) == SOCKET_ERROR) throw_wsa("listen failed");
    return s;
}

// 从监听套接字接受一个连接
SOCKET tcp_accept(SOCKET listenSock) {
    SOCKET c = accept(listenSock, nullptr, nullptr);
    if (c == INVALID_SOCKET) throw std::runtime_error("accept failed");
    return c;
}

// 主动连接到指定 IP/端口
SOCKET tcp_connect(const char* ip, uint16_t port) {
    SOCKET s = mk_socket();
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) throw std::runtime_error("inet_pton failed");

    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) throw_wsa("connect failed");
    return s;
}

// 发送指定长度的数据，直到发完
bool send_all(SOCKET s, const void* data, size_t bytes) {
    // 循环发送直到字节数耗尽，确保完整发出
    const char* p = (const char*)data;
    while (bytes) {
        int n = send(s, p, (int)bytes, 0);
        if (n <= 0) return false;
        p += n; bytes -= (size_t)n;
    }
    return true;
}

// 接收指定长度的数据，直到收满
bool recv_all(SOCKET s, void* data, size_t bytes) {
    // 循环接收直到拿满指定字节，确保数据完整
    char* p = (char*)data;
    while (bytes) {
        int n = recv(s, p, (int)bytes, 0);
        if (n <= 0) return false;
        p += n; bytes -= (size_t)n;
    }
    return true;
}

// 安全关闭套接字
void close_sock(SOCKET s) {
    if (s != INVALID_SOCKET) closesocket(s);
}
