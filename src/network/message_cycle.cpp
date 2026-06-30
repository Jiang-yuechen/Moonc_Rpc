#include "message_cycle.h"
#include "create_socket.h"
#include "../protocol/rpc_protocol.h"
#include "../log/log_manager.h"
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace mooncrpc {

MessageCycle::MessageCycle(const std::string& ip, uint16_t port) 
    : is_running_(false), ip_(ip), port_(port)
{
    // 1. 创建 epoll 实例
    // epoll_create1(0) 和旧的 epoll_create 类似，但接口更现代
    // 它在内核中开辟了一块空间 (包含一个红黑树来存 FD 以及一个双向链表来存就绪事件)
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        LOG_FATAL("Failed to create epoll instance: {}", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // 2. 创建并初始化服务端的监听 Socket
    listen_fd_ = SocketUtils::CreateTcpSocket();
    SocketUtils::SetReuseAddr(listen_fd_);      // 允许立刻重用 TIME_WAIT 状态的端口
    SocketUtils::SetNonBlocking(listen_fd_);    // 设置为非阻塞，配合 epoll 边缘触发
    SocketUtils::Bind(listen_fd_, ip_, port_);
    SocketUtils::Listen(listen_fd_, 1024);

    // 3. 将监听 Socket 挂载到 epoll 的红黑树上
    struct epoll_event event;
    event.data.fd = listen_fd_;

    // 关注可读事件以及设置为边缘触发
    // 边缘触发：只有当状态发生变化(eg.新客户端刚连上时)，epoll 才会唤醒你一次
    event.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &event) < 0) {
        LOG_FATAL("Failed to add listen_fd to epoll: {}", strerror(errno));
        exit(EXIT_FAILURE);
    }

    LOG_INFO("MessageCycle initialized. Epoll engine listening on {}:{}", ip_, port_);
}


MessageCycle::~MessageCycle() {
    Stop();
    SocketUtils::Close(listen_fd_);
    SocketUtils::Close(epoll_fd_);
}

void MessageCycle::Start() {
    is_running_ = true;
    struct epoll_event events[MAX_EVENTS];  //用于接收内核传回来的就绪事件数组

    LOG_INFO("MessageCycle started. Reactor event loop is running...");

    // 事件死循环：霸占当前线程(IO 线程/ Reactor 线程)
    while (is_running_) {
        // epoll_wait 阻塞当前线程
        // 它会检查内核的双向链表里有没有就绪事件。若有则立刻把事件拷贝到 events 数据并返回事件数量
        // -1 表示死等直到有事件发生或者被 OS 的信号打断
        int num_events = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        if (num_events < 0) {
            // EINTR 是被调试工具或系统信号打断的，继续即可
            if (errno == EINTR) {
                continue;
            }
            LOG_ERROR("epoll_wait error: {}", strerror(errno));
            break;
        }

        // 遍历所有被激活的 Socket
        for (int i = 0; i < num_events; i++) {
            int active_fd = events[i].data.fd;

            // Linux 将状态压缩进一个 32bit 整数中，每个事件都独占一个二进制位
            uint32_t active_events = events[i].events;

            if (active_fd == listen_fd_) {
                // 监听 Socket 活跃，说明有新客户端建立连接
                HandleAccept();
            } else {
                // 普通客户端的 Socket 活跃，先检查是不是异常断开
                // 异步错误 | 异常崩溃 | 对端优雅关闭连接
                if (active_events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    LOG_WARN("FD {} triggered error/disconnect event.", active_fd);
                    ConnectionManager::GetInstance().RemoveConnection(active_fd);
                    continue;
                }

                // 收到客户端发来的真实数据
                if (active_events & EPOLLIN) {
                    HandleRead(active_fd);
                }

                // 客户端网卡的发送缓冲区有空位了，可以继续把服务器积压的数据发走
                if (active_events & EPOLLOUT) {
                    HandleWrite(active_fd);
                }
            }
        }
    }
}

void MessageCycle::Stop() {
    is_running_ = false;
}

void MessageCycle::HandleAccept() {
    // 因为配置了边缘触发，所以一次性 Accept 所有连接
    while (1) {
        std::string client_ip;
        uint16_t client_port;
        // Accept 内部会自动处理 EAGAIN， 当没有新连接是返回 -1
        int client_fd = SocketUtils::Accept(listen_fd_, client_ip, client_port);

        if (client_fd < 0) {
            // 所有连接全部处理完毕，退出循环
            break;
        }

        // 封装成智能指针，交给连接管理器统一接管生命周期
        auto conn = std::make_shared<TcpConnection>(client_fd, client_ip, client_port);
        ConnectionManager::GetInstance().AddConnection(conn);

        // 将新连接也挂载到 epoll 上
        // EPOLLRDHUP：让底层 TCP 协议栈去探测对端是否关闭连接
        struct epoll_event event;
        event.data.fd = client_fd;
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &event);
    }
}

void MessageCycle::HandleRead(int fd) {
    // 获取连接
    auto conn = ConnectionManager::GetInstance().GetConnection(fd);
    if (!conn) {
        return;
    }

    // 触发内部的非阻塞死循环读取直到将接收缓冲区数据全部存入 conn 的 in_buffer_
    int bytes_read = conn->Read();
    if (bytes_read <= 0) {
        // 客户端关闭连接或者发生错误
        ConnectionManager::GetInstance().RemoveConnection(fd);
        return;
    }

    // 数据已就绪，开始进行网络切包与业务分发
    ProcessBuffer(conn);
}

void MessageCycle::ProcessBuffer(std::shared_ptr<TcpConnection> conn) {
    // 处理粘包问题
    // 一次网络事件可能包含了客户端拼在一起发过来的好几个完整的 RPC 请求
    while (true) {
        RpcMessage req_msg;
        // RpcProtocol::Decode 会根据 20B 的包头进行半包校验
        // 如果数据不够拼成一个完整的包就返回false
        if (!RpcProtocol::Decode(conn->GetReadBuffer(), req_msg)) {
            // 发生半包，将数据留在 buffer 中，等下次 EPOLLIN 事件凑齐一个包再解码
            break;
        }

        // 完美切出一个包，算出这个包在缓冲区里占用的总长度，并把他们从读缓冲区中移除
        size_t consumed_size = sizeof(RpcHeader) + req_msg.header.body_size;
        conn->ClearReadBuffer(consumed_size);

        // 提取路由名称(最终应该从 Protobuf/JSON 反序列化后的结构体中动态提取)
        std::string service_name = "UserService";
        
        int fd = conn->GetFd();

        // 核心分发逻辑：网络线程不管业务，直接把组装好的包扔给异步线程池
        // Lambda 捕获 conn 和 fd，这样哪怕一会执行完毕也能找到归属的连接
        ServiceManager::GetInstance().HandleRpcRequestAsync(service_name, req_msg,
            [this, conn, fd](const RpcMessage& res_msg) {
                // 在 ThreadPool 子线程中运行
                std::string encoded_res;
                if (RpcProtocol::Encode(res_msg, encoded_res)) {
                    // 1. 将加密打包好的响应报文追加到该连接的私有写缓冲区中
                    conn->AppendToWriteBuffer(encoded_res);

                    // 2. 跨线程通信
                    // 子线程把数据塞进缓冲区后必须通知主 Reactor/IO 线程去发数据
                    // 动态修改 epoll 监听事件，追加 EPOLLOUT (可写事件)
                    // 主线程的 epoll_wait 察觉到后就会立刻触发 HanderWrite 进行发送
                    struct epoll_event ev;
                    ev.data.fd = fd;
                    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
                    epoll_ctl(this->epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
                }
            }
        );
    }
}

void MessageCycle::HandleWrite(int fd) {
    auto conn = ConnectionManager::GetInstance().GetConnection(fd);
    if (!conn) {
        return;
    }

    // 触发非阻塞传送，尽可能多地把 out_buffer_ 里的数据推入内核发送队列
    int bytes_sent = conn->Write();
    if (bytes_sent < 0) {
        ConnectionManager::GetInstance().RemoveConnection(fd);
        return;
    }

    // 取消 EPOLLOUT
    // 如果把 out_buffer_ 里的数据全部发完了就必须向内核取消对 EPOLLOUT 的监听
    // 因为只要网卡发送缓冲区不满，它就一直是“可写”状态。
    // 如果你注册了 EPOLLOUT 又一直没有新数据要发，epoll 会陷入无限触发的死循环
    if (conn->GetState() == ConnectionState::CONNECTED) {
        // 若内部的维护的发送队列为空 (需要在 TcpConnection 类中实现判断逻辑)
        // 这里每次写完就取消 EPOLLOUT，由后续新的 Response 来重新激活
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = EPOLLIN | EPOLLET |EPOLLRDHUP;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    }
}

} // namespace mooncrpc