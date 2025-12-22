
/**
 * @file net.cpp
 * @brief 网络通信库实现
 * * 实现了 net.h 中声明的网络接口。
 * 负责处理 WSAStartup/WSACleanup 的生命周期管理，
 * 并封装了底层的 socket、bind、listen、connect、send、recv 等系统调用，
 * 为上层业务逻辑提供简单的阻塞式 TCP 数据传输服务。
 */
#pragma comment(lib, "Ws2_32.lib")

#include "net.h"
#include <stdexcept>

// 初始化/清理 WinSock2
// 通过 RAII 管理 WSAStartup/WSACleanup 的生命周期，避免忘记清理。
WsaInit::WsaInit() {
    // 要求使用 WinSock 2.2
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) throw std::runtime_error("WSAStartup failed");
}
WsaInit::~WsaInit() { WSACleanup(); }

// 创建基础 TCP 套接字
// 失败时抛异常，调用方无需检查 INVALID_SOCKET。
static SOCKET mk_socket() {
    // 创建 TCP 流式套接字
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) throw std::runtime_error("socket failed");
    return s;
}

// net.cpp
#include <sstream>

// 将 WSAGetLastError() 包装进异常消息，便于定位 WinSock 错误原因
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
    // 本地端口号需要网络字节序
    addr.sin_port = htons(port);

    // 允许地址复用：服务端重启时避免 TIME_WAIT 导致 bind 失败
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // 绑定并进入监听状态，失败则抛出带 WSA 错误码的异常
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) throw_wsa("bind failed");
    if (listen(s, 1) == SOCKET_ERROR) throw_wsa("listen failed");
    return s;
}

// 从监听套接字接受一个连接
SOCKET tcp_accept(SOCKET listenSock) {
    // 阻塞等待客户端连接；忽略对端地址信息
    SOCKET c = accept(listenSock, nullptr, nullptr);
    if (c == INVALID_SOCKET) throw std::runtime_error("accept failed");
    return c;
}

// 主动连接到指定 IP/端口
SOCKET tcp_connect(const char* ip, uint16_t port) {
    SOCKET s = mk_socket();
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    // 目标端口转换为网络字节序
    addr.sin_port = htons(port);
    // 将点分十进制 IP 转换为网络地址结构
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) throw std::runtime_error("inet_pton failed");

    // connect 失败时带上 WSA 错误码
    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) throw_wsa("connect failed");
    return s;
}

// 发送指定长度的数据，直到发完
bool send_all(SOCKET s, const void* data, size_t bytes) {
    // 循环发送直到字节数耗尽，确保完整发出
    const char* p = (const char*)data;
    while (bytes) {
        // send 可能出现部分发送，因此需要累计推进指针
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
        // recv 可能出现部分接收，需循环直到满足长度
        int n = recv(s, p, (int)bytes, 0);
        if (n <= 0) return false;
        p += n; bytes -= (size_t)n;
    }
    return true;
}

// 安全关闭套接字
void close_sock(SOCKET s) {
    // 忽略 INVALID_SOCKET，避免重复关闭
    if (s != INVALID_SOCKET) closesocket(s);
}
