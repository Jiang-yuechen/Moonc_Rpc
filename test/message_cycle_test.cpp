#include "../src/network/message_cycle.h"
#include "../src/network/create_socket.h"
#include "../src/protocol/rpc_protocol.h"
#include "../src/log/log_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace mooncrpc;

// 1. 定义一个简单的业务服务用于测试
class MockUserService : public Service {
public:
    std::string GetServiceName() const override { 
        return "UserService";
    }

    void HandleRpcRequest(const RpcMessage& req, RpcMessage& res) override {
        LOG_INFO("MockUserService handling request in worker thread... Payload: {}", req.body);
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 模拟业务耗时
        res.header.error_code = 0;
        res.body = "Welcome to MooncRPC! Your payload was: " + req.body;
    }
};

int main() {
    if (!Logger::GetInstance().Init("./test_logs")) {
        return -1;
    }

    LOG_INFO("====== Starting Epoll Message Cycle Ultimate Test ======");

    // 1. 初始化并发大脑 (ThreadPool) 和路由总线 (ServiceManager)
    auto thread_pool = std::make_shared<ThreadPool>(4);
    ServiceManager::GetInstance().Init(thread_pool);
    ServiceManager::GetInstance().RegisterService(std::make_shared<MockUserService>());

    // 2. 启动 Reactor 事件循环引擎 (在后台线程运行)
    MessageCycle server_reactor("127.0.0.1", 8989);
    std::thread reactor_thread([&server_reactor]() {
        server_reactor.Start();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待服务启动

    // ---------------------------------------------------------
    // 主线程扮演客户端，发起真实的 RPC 报文
    // ---------------------------------------------------------
    int client_fd = SocketUtils::CreateTcpSocket();
    if (SocketUtils::Connect(client_fd, "127.0.0.1", 8989)) {
        LOG_INFO("[Client] Connected to server successfully.");

        // 构造合法的 RPC 请求信封
        RpcMessage req;
        req.header.seq_id = 9527;
        req.body = "Hello Epoll Engine!";

        std::string req_buffer;
        RpcProtocol::Encode(req, req_buffer);

        // 发送数据给服务端
        ::send(client_fd, req_buffer.data(), req_buffer.size(), 0);
        LOG_INFO("[Client] Sent encoded RPC Request. Total size: {} bytes", req_buffer.size());

        // 阻塞等待服务端的处理回包
        char recv_buf[1024] = {0};
        ssize_t bytes_recv = ::recv(client_fd, recv_buf, sizeof(recv_buf), 0);
        
        if (bytes_recv > 0) {
            // 解析服务端的返回包
            std::string res_stream(recv_buf, bytes_recv);
            RpcMessage res;
            if (RpcProtocol::Decode(res_stream, res)) {
                LOG_INFO("[Client] Decoded Response SUCCESS! SeqId: {}, Body: {}", 
                         res.header.seq_id, res.body);
            } else {
                LOG_ERROR("[Client] Failed to decode server response.");
            }
        }
    }

    // 清理与退出
    SocketUtils::Close(client_fd);
    server_reactor.Stop();
    // 强制唤醒一下 epoll 以便安全退出
    int dummy_fd = SocketUtils::CreateTcpSocket();
    SocketUtils::Connect(dummy_fd, "127.0.0.1", 8989);
    SocketUtils::Close(dummy_fd);
    
    reactor_thread.join();
    thread_pool->Shutdown();
    
    LOG_INFO("====== Message Cycle Test Completed Successfully ======");
    return 0;
}