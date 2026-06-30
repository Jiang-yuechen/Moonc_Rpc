#include "../src/log/log_manager.h"
#include "../src/network/connection.h"
#include "../src/network/create_socket.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace mooncrpc;

// 模拟服务器线程：接收连接 -> 读取数据 -> 原样返回
void MockServerThread() {
    int listen_fd = SocketUtils::CreateTcpSocket();
    SocketUtils::SetReuseAddr(listen_fd);
    SocketUtils::SetNonBlocking(listen_fd);
    SocketUtils::Bind(listen_fd, "127.0.0.1", 9090);
    SocketUtils::Listen(listen_fd, 10);

    LOG_INFO("[Server] Listening on 127.0.0.1:9090...");

    std::string client_ip;
    uint16_t client_port;
    int client_fd = -1;

    // 轮询等待客户端接入(因为非阻塞，所以必须用 while 循环)
    while (client_fd < 0) {
        client_fd = SocketUtils::Accept(listen_fd, client_ip, client_port);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 把裸露的 FD 包装成 TcpConnection
    TcpConnection conn(client_fd, client_ip, client_port);

    // 等待客户端发送数据
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 执行非阻塞读取
    int bytes_read = conn.Read();
    LOG_INFO("[Server] Read {} bytes into buffer. Buffer content: {}", bytes_read, conn.GetReadBuffer());

    // 测试写逻辑：把读到的数据塞进写缓冲区再写出去
    conn.AppendToWriteBuffer("Server Echo -> " + conn.GetReadBuffer());
    conn.ClearReadBuffer(bytes_read);

    int bytes_written = conn.Write();
    LOG_INFO("[Server] Sent {} bytes back to client.", bytes_written);

    SocketUtils::Close(listen_fd);

}

int main() {
    if (!Logger::GetInstance().Init("./test_logs")) {
        return -1;
    }

    LOG_INFO("====== Starting TcpConnection Module Test ======");

    // 1. 启动服务器线程
    std::thread server_thread(MockServerThread);

    // 稍微等一下，让服务器 Listen 准备好
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 2. 主线程扮演客户端，主动连接
    int client_fd = SocketUtils::CreateTcpSocket();
    SocketUtils::Connect(client_fd, "127.0.0.1", 9090);
    
    // 客户端发数据
    std::string payload = "Hello MooncRPC Network Layer!";
    ::send(client_fd, payload.data(), payload.size(), 0);
    LOG_INFO("[Client] Sent message to server.");

    // 客户端等待回音
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    char buf[1024] = {0};
    ::recv(client_fd, buf, sizeof(buf), 0);
    LOG_INFO("[Client] Received reply from server: {}", buf);

    SocketUtils::Close(client_fd);
    server_thread.join();
    
    LOG_INFO("====== TcpConnection Test Completed Successfully ======");
    return 0;
}