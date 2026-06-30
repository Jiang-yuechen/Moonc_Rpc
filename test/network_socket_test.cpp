#include "../src/log/log_manager.h"
#include "../src/network/create_socket.h"
#include <iostream>

using namespace mooncrpc;

int main() {
    if (!Logger::GetInstance().Init("./test_logs")) {
        return -1;
    }

    LOG_INFO("====== Starting Low-Level Socket Utility Test ======");

    // 1. 创建 TCP Socket
    int server_fd = SocketUtils::CreateTcpSocket();
    if (server_fd < 0) {
        return -1;
    }

    // 2. 设置端口复用和非阻塞模式
    SocketUtils::SetReuseAddr(server_fd);
    SocketUtils::SetNonBlocking(server_fd);

    // 3. 绑定并监听本地 8989 端口
    std::string bind_ip = "0.0.0.0";
    uint16_t bind_port = 8989;

    if (!SocketUtils::Bind(server_fd, bind_ip, bind_port)) {
        SocketUtils::Close(server_fd);
        return -1;
    }

    if (!SocketUtils::Listen(server_fd, 128)) {
        SocketUtils::Close(server_fd);
        return -1;
    }

    LOG_INFO("Mock Server is now listening on {}:{}.", bind_ip, bind_port);

    // 4. 清理内核资源
    SocketUtils::Close(server_fd);

    LOG_INFO("===== Socket Utility Test Completed =====");
    return 0;
}