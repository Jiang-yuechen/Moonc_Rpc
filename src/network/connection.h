#pragma once
#include <string>
#include <memory>
#include <cstdint>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace mooncrpc {

// 连接状态机
enum class ConnectionState {
    CONNECTED,  // 正常连接中
    CLOSED      // 已断开
};

/**
 * @brief TCP 连接封装类
 * @details 负责管理单个 Socket 的生命周期、读写缓冲区以及非阻塞 I/O 的具体流转。
 * 继承 std::enable_shared_from_this 是为了在异步回调中安全的延长自身生命周期
 */
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(int fd, const std::string& peer_ip, uint16_t peer_port);
    ~TcpConnection();

    /**
     * @brief 核心：非阻塞读取网络数据
     * @details 会一直循环读取，直到内核缓冲区被掏空(返回 EAGAIN)
     * @return 成功读取的总字节数。返回 0 表示对端断开，返回 -1 表示发生致命错误
     */
    int Read();

    /**
     * @brief 核心：非阻塞发送网络数据
     * @details 会将 write_buffer_ 中的数据尽可能发出去，发不完的留在 buffer 中等待下次 epoll 触发
     * @return 成功发送的字节数。-1 表示发生致命错误
     */
    int Write();

    /**
     * @brief 将业务层准备好的数据塞入发送缓冲区
     * @param data 已经通过协议层 Encode 打包好的完整二进制流
     */
    void AppendToWriteBuffer(const std::string& data);

    // 获取读缓冲区的引用 (供协议层 Decode 切包使用)
    std::string& GetReadBuffer() {
        return read_buffer_;
    }

    // 当协议层成功切走一个完整的包后，调用此函数清除已解析的数据
    void ClearReadBuffer(size_t size);

    // 主动关闭连接
    void Close();

    // Getter 方法
    ConnectionState GetState() const { return state_; }
    int GetFd() const { return fd_; }
    std::string GetPeerIp() const { return peer_ip_; }
    uint16_t GetPeerPort() const { return peer_port_; }

private:
    int fd_;                    // Socket 句柄
    ConnectionState state_;     // 当前连接状态
    std::string peer_ip_;       // 对端 IP
    uint16_t peer_port_;        // 对端端口

    std::string read_buffer_;   // 读缓冲区：存放接收到但还没被协议层切走的半包数据
    std::string write_buffer_;  // 写缓冲区：存放准备发送但网卡还没来得及发出去的数据
    std::mutex write_mutex_;    // 保护 write_buffer_ 的互斥锁
};

} //namespace mooncrpc