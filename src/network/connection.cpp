#include "connection.h"
#include "create_socket.h"
#include "../log/log_manager.h"

namespace mooncrpc {

TcpConnection::TcpConnection(int fd, const std::string& peer_ip, uint16_t peer_port)
    : fd_(fd), state_(ConnectionState::CONNECTED), peer_ip_(peer_ip), peer_port_(peer_port)
{
    // 一旦接管 fd，立即强制确保其为非阻塞模式
    SocketUtils::SetNonBlocking(fd_);
    LOG_DEBUG("TcpConnection created. FD: {}, Peer: {}:{}", fd_, peer_ip_, peer_port_);
}

TcpConnection::~TcpConnection() {
    Close();
}

int TcpConnection::Read() {
    if (state_ != ConnectionState::CONNECTED) {
        return -1;
    }

    char buf[4096];     // 每次读取 4KB 数据的本地栈
    int total_read = 0;

    // 只要是边缘触发(ET)非阻塞模式，就必须一口气把网卡中的数据读干
    while (1) {
        ssize_t bytes_read = ::recv(fd_, buf, sizeof(buf), 0);
        if (bytes_read > 0) {
            // 读到数据就放入读缓冲区
            read_buffer_.append(buf, bytes_read);
            total_read += bytes_read;
        } else if (bytes_read == 0) {
            // recv 返回 0 的唯一含义：对端(客户端)正常执行了 close()
            LOG_INFO("Peer {}:{} closed the connection.", peer_ip_, peer_port_);
            Close();
            return 0;
        } else {
            // bytes_read < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 网卡接收缓冲区数据全部读完，等待下次数据
                break;
            } else if (errno == EINTR) {
                // 读取过程中被 Linux 系统的底层信号中断了，忽略并重试即可
                continue;
            } else {
                // 真正遇到网络错误
                LOG_ERROR("Read error on FD {}: {}", fd_, strerror(errno));
                Close();
                return -1;
            }
        }
    }
    return total_read;
}

void TcpConnection::AppendToWriteBuffer(const std::string& data) {
    // 加锁保护，因为这个函数是在 ThreadPool 的子线程中被调用的
    std::lock_guard<std::mutex> lock(write_mutex_);
    write_buffer_.append(data);
}

int TcpConnection::Write() {
    if (state_ != ConnectionState::CONNECTED || write_buffer_.empty()) {
        return 0;
    }

    // 加锁保护，因为这个函数是在 Reactor 的主事件线程中被调用的
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    if (write_buffer_.empty()) return 0;

    int total_sent = 0;
    
    // 只要写缓冲区还有数据就不断往网卡塞
    while (!write_buffer_.empty()) {
        ssize_t bytes_sent = ::send(fd_, write_buffer_.data(), write_buffer_.size(), 0);
        if (bytes_sent > 0) {
            total_sent += bytes_sent;
            // 塞成功多少就从写缓冲区删除多少
            write_buffer_.erase(0, bytes_sent);
        } else if (bytes_sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 网卡的发送队列满了，TCP 进行拥塞控制
                // 直接 break 退出去，剩下的数据依然保存在 write_buffer_ 里
                // 下次 epoll 时再把剩下的发完
                LOG_DEBUG("Write buffer full on FD {}, will resume later.", fd_);
                break;
            } else if (errno == EINTR) {
                //信号被打断，重试
                continue;
            } else {
                //对端可能已经挂了，导致发不出去
                LOG_ERROR("Write error on FD {}: {}", fd_, strerror(errno));
                Close();
                return -1;
            }
        }
    }
    return total_sent;
}

void TcpConnection::ClearReadBuffer(size_t size) {
    if (size >= read_buffer_.size()) {
        read_buffer_.clear();
    } else {
        read_buffer_.erase(0, size);
    }
}

void TcpConnection::Close() {
    // 已经关闭连接，直接返回
    if (state_ == ConnectionState::CLOSED) {
        return;
    }
    
    state_ = ConnectionState::CLOSED;
    SocketUtils::Close(fd_);
    LOG_DEBUG("TcpConnection closed. FD: {}", fd_);
}

} //namespace mooncrpc