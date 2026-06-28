#pragma once
#include "connection.h"
#include <unordered_map>
#include <memory>
#include <mutex>

namespace mooncrpc {

/**
 * @brief 全局 TCP 连接管理器 (单例模式)
 * @details 负责集中管理所有的 TcpConnection 生命周期
 * 它是 epoll 获取到的 fd 与面向实例之间的桥梁
 */    
class ConnectionManager {
public:
    static ConnectionManager& GetInstance() {
        static ConnectionManager instance;
        return instance;
    }

    // 禁用拷贝与赋值
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // 添加一个新连接到管理器中
    void AddConnection(std::shared_ptr<TcpConnection> conn);

    // 根据 fd 移除并销毁一个连接
    void RemoveConnection(int fd);

    // 根据 fd 查找连接对象
    std::shared_ptr<TcpConnection> GetConnection(int fd);

    // 强制断开并清理所有连接
    void ClearAll();

private:
    ConnectionManager() = default;
    ~ConnectionManager() {
        ClearAll();
    }

    // 核心哈希表： FD -> TcpConnection 实例
    std::unordered_map<int, std::shared_ptr<TcpConnection>> connections_;

    // 保护哈希表的锁
    mutable std::mutex mutex_;
};

} // namespace mooncrpc