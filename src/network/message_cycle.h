#pragma once
#include "../service/service_manager.h"
#include "connection_manager.h"
#include <string>
#include <sys/epoll.h>
#include <memory>
#include <atomic>

namespace mooncrpc {

/**
 * @brief 网络事件循环 (Reactor 模式)
 * @details 基于 Linux epoll 实现的单线程/多线程事件分发器
 * 负责监听新连接、读写事件，并将解码后的数据包派发给业务线程池
 */
class MessageCycle {
public:
    // 构造函数：需要传入服务端准备监听的本地 IP 和端口
    MessageCycle(const std::string& ip, uint16_t port);

    // 析构函数：负责关闭底层的 epoll_fd 和 listen_fd，释放内核资源
    ~MessageCycle();

    // 启动事件死循环 (将阻塞当前线程)
    void Start();

    // 停止事件循环
    void Stop();

private:
    int epoll_fd_;      // epoll 句柄
    int listen_fd_;     // 服务端监听 Socket
    bool is_running_;   // 循环状态标志
    std::string ip_;
    uint16_t port_;

    // 每次 epoll_wait 最多拉取 1K 个活跃事件
    static constexpr int MAX_EVENTS = 1024;

    // 处理客户端新连接
    void HandleAccept();

    // 处理已有连接的可读事件
    void HandleRead(int fd);
    
    // 处理已有连接的可写事件
    void HandleWrite(int fd);

    // 核心切包与异步分发逻辑
    // 当 HandleRead 读到一堆乱七八糟的字节流后，它会调用这个方法。
    // 这个方法负责：
    // 1. 解决 TCP 粘包/半包问题（识别 RpcHeader）。
    // 2. 将完整的网络字节流反序列化为 RpcMessage 结构体。
    // 3. 将 RpcMessage 丢给 ServiceManager::HandleRpcRequestAsync，扔进线程池异步执行业务逻辑。
    void ProcessBuffer(std::shared_ptr<TcpConnection> conn);
};

} // namespace mooncrpc