#include "../log/log_manager.h"
#include "connection_manager.h"

namespace mooncrpc {

// 登记新连接：按值传递指针会增加一次引用计数,确保只要 Manager 还持有它，这个连接的内存就不会被释放
void ConnectionManager::AddConnection(std::shared_ptr<TcpConnection> conn) {
    if (!conn) {
        return;
    }

    // RAII 风格保护连接器
    std::lock_guard<std::mutex> lock(mutex_);

    // 以文件描述符作为唯一标识 Key
    int fd = conn->GetFd();
    connections_[fd] = conn;
    LOG_DEBUG("Connection added to manager. FD: {}, Total Active Connections: {}", fd, connections_.size());
}

// 移除注销连接
void ConnectionManager::RemoveConnection(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        // 1. 先调用底层逻辑，通过系统调用 ::close(fd) 正式关闭网络套接字，断开 TCP 连接，回收系统资源
        it->second->Close();

        // 2. 将对象从 map 中擦除，同时这会导致 TcpConnection 的 shared_ptr 引用计数 -1
        connections_.erase(it);

        LOG_DEBUG("Connection removed from manager. FD: {}, remaining connections: {}", fd, connections_.size());
    } else {
        LOG_WARN("Attempted to remove non_existent connection. FD: {}", fd);
    }
}

// 查找并获取连接
std::shared_ptr<TcpConnection> ConnectionManager::GetConnection(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

// 清理所有连接
void ConnectionManager::ClearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : connections_) {
        // 关闭所有连接
        pair.second->Close();
    }

    connections_.clear();
    LOG_INFO("All connections cleared from manager.");
}

} // namespace mooncrpc