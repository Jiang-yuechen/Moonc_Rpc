#include "create_socket.h"
#include "../log/log_manager.h"

//Linux POSIX Socket 头文件
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

namespace mooncrpc {

int SocketUtils::CreateTcpSocket() {
    // 使用 :: 代表让编译器去全局作用域寻找 socket 函数，避免命名冲突
    // ::socket() 系统调用向内核申请分配一个 Socket 资源。
    // AF_INET: 指定地址族为 IPv4 (如果是 IPv6 则为 AF_INET6)
    // SOCK_STREAM: 指定套接字类型为面向连接的、可靠的流式传输，底层即 TCP 协议
    // 0: 默认协议，对于 SOCK_STREAM 来说就是 TCP
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERROR("Failed to create TCP socket. Error: {}", strerror(errno));
        return -1;
    }
    LOG_DEBUG("Successfully created TCP socket, fd: {}", fd);
    return fd;
}

bool SocketUtils::Bind(int fd, const std::string& ip, uint16_t port) {
    // sockaddr_in 是专门用于处理 IPv4 地址的结构体
    struct sockaddr_in addr;

    // 初始化清零。 sockaddr_in 结构体中为了与通用的 sockaddr 大小对齐，
    // 包含了一段 padding 字段。如果不清零，里面可能含有脏数据，导致 bind 失败。
    std::memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;

    // htons = Host TO Network Short。
    // 主机字节序（通常是小端序）转换为网络字节序（严格规定为大端序）。
    // 如果不转换，端口号在网络上传输时会被解析错乱。
    addr.sin_port = htons(port);

    // inet_pton = Presentation TO Numeric。
    // 将人类可读的字符串 IP (如 "192.168.1.1") 转换为网络字节序的 32 位二进制整数。
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid IP address format: {}", ip);
        return false;
    }

    // ::bind() 将上面配置好的 IP 和端口真正绑定到内核的 fd 上。
    // 因为 bind API 是很早以前设计的，当时没有 void*，所以它要求接收一个通用的 sockaddr 结构体指针。
    // 这里必须使用 reinterpret_cast 进行强转。
    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("Failed to bind socket to {}:{}. Error: {}", ip, port, strerror(errno));
        return false;
    }

    LOG_INFO("Successfully bound socket to {}:{}", ip, port);
    return true;
}

bool SocketUtils::Listen(int fd, int backlog) {
    // ::listen() 将一个主动发起连接的 socket 转换为被动接收连接的监听 socket。
    // backlog 参数：指定内核中已完成三次握手（ESTABLISHED 状态）但还没被应用程序 accept 取走的连接全连接队列(Accept Queue)的最大长度。
    // 如果瞬间并发量过大把队列塞满，客户端再发起握手就会被拒绝。
    if (::listen(fd, backlog) < 0) {
        LOG_ERROR("Failed to listen on socket. Error: {}", strerror(errno));
        return false;
    }
    LOG_INFO("Socket is now listening with backlog: {}", backlog);
    return true;
}

int SocketUtils::Accept(int fd, std::string& client_ip, uint16_t& client_port) {
    struct sockaddr_in client_addr;

    // addr_len 是“值-结果”参数
    // 传入结构体大小，内核填完客户端数据后会把它修改为直接写入的大小
    socklen_t addr_len = sizeof(client_addr);

    // ::accept() 从全连接队列中取一个已经完成三次握手的客户端连接
    // 返回值 client_fd 专门用来和这个特定的客户端进行数据收发
    // 原来的 fd 依然在后台专门负责监听新连接
    int client_fd = ::accept(fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 非阻塞 I/O 模式下的假错误
            // 如果监听 socket 被设置为非阻塞，当全连接队列为空时， accept 会立即返回 -1
            // 此时系统错误码被设置为 EAGAIN 或 EWOULDBLOCK。其含义仅表示当前没有任务
            return -1;
        }
        LOG_ERROR("Failed to accept new connection. Error: {}", strerror(errno));
        return -1;
    }

    // 提取客户端 IP 和端口，用于日志记录或鉴权
    char ip_buf[INET_ADDRSTRLEN];
    // inet_ntop 把内核返回的 32 位整数翻译回字符串 IP
    ::inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
    client_ip = static_cast<std::string>(ip_buf);

    // ntohs: 把大端序端口号转回本地小端序整数
    client_port = ntohs(client_addr.sin_port);

    LOG_INFO("Accepted new connection from {}:{}, assigned fd: {}", client_ip, client_port, client_fd);
    return client_fd;
}

bool SocketUtils::Connect(int fd, const std::string& server_ip, uint16_t server_port) {
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (::inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid server IP address format: {}", server_ip);
        return false;
    }

    // ::connect() 由客户端主动发起 TCP 三次握手
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        // 非阻塞 Connect
        // 对于非阻塞 socket，调用 connect 后，由于三次握手需要时间(发 SYN、等待 ACK)，
        // 函数会立刻返回失败，并将 errno 设置为 EINPROGRESS(进行中)
        // 此时我们呢需要把这个 fd 挂到 epoll/select 上，监听它的“可写事件(EPOLLOUT)”来判断连接是否最终建立成功。
        if (errno == EINPROGRESS) {
            LOG_DEBUG("Connection is in progress(non-blocking).");
            return true;
        }
        LOG_ERROR("Failed to connect to {}:{}. Error: {}", server_ip, server_port, strerror(errno));
        return false;
    }

    LOG_INFO("Successfully connected to {}:{}", server_ip, server_port);
    return true;
}

bool SocketUtils::SetNonBlocking(int fd) {
    // 经典的“读-改-写”三部曲
    // 1. F_GETFL: 获取 fd 当前的状态标志位 (Flags)
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        LOG_ERROR("Failed to get socket flags for fd: {}. Error:{}", fd, strerror(errno));
        return false;
    }

    // 2. F_SETFL: 按位或运算，在保留原有标志位的基础上，追加 O_NONBLOCK 非阻塞标志
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        LOG_ERROR("Failed to set non-blocking flag for fd: {}. Error: {}", fd, strerror(errno));
        return false;
    }
    LOG_DEBUG("Socket fd: {} configured as non-blocking.", fd);
    return true;
}

bool SocketUtils::SetReuseAddr(int fd) {
    int optval = 1;
    // 处理 TIME_WAIT 状态
    // 当服务端主动关闭连接或者异常崩溃重启时，底层 TCP 连接会进入 TIME_WAIT 状态(持续约 2MSL，通常1-2min)
    // 在这段事件内，该端口是被系统占用的，如果服务端立刻重启并尝试 bind 该端口，会报 "Address already in use" 错误
    // 开启 SO_REUSEADDR 选项允许绕过这个限制，立即重用该端口，这是服务端程序的必备配置
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        LOG_ERROR("Failed to set SO_REUSEADDR for fd: {}. Error: {}", fd, strerror(errno));
        return false;
    }
    LOG_DEBUG("Socket fd: {} configured with SO_REUSEADDR.", fd);
    return true;
}

void SocketUtils::Close(int fd) {
    if (fd > 0) {
        // ::close() 会减少 fd 的引用计数，只有计数为 0 时才真正拆除网络连接并回收资源
        // 由于 Linux 系统对单个进行的可打开文件描述符有上限(默认一般是 1024)
        // 如果忘记 close，会导致 fd 泄漏，最终出现 "Too many open files" 崩溃
        ::close(fd);
        LOG_DEBUG("Socket fd: {} closed.", fd);
    }
}

} //namespace mooncrpc