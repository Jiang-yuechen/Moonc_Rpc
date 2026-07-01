#pragma once

#include <string>
#include <memory>

namespace mooncrpc {

// 前置声明：避免在头文件中引入大量底层模块的依赖
class Service;
class ServiceRegistry;
class MessageCycle;

/**
 * @brief 框架服务端最终门面类
 * @details 负责整体架构的生命周期管理:
 * 加载配置、初始化线程池、服务注册、启动 Reactor
 */
class RpcServer {
public:
    // 构造函数：接管整个服务端从零到一的启动逻辑
    explicit RpcServer(const std::string& config_file);

    // 禁用拷贝和赋值，保证服务器实例全局唯一
    RpcServer(const RpcServer&) = delete;
    RpcServer& operator=(const RpcServer&) = delete;

    // 析构函数：优雅停机
    ~RpcServer();

    // 暴露给用户的接口：注册本地业务服务
    void RegisterService(std::shared_ptr<Service> service);

    // 暴露给用户的接口：启动事件循环 (将阻塞主线程)
    void Start();
private:
    std::string ip_;            // 服务端监听 IP
    uint16_t port_;             // 服务端监听端口

    // 使用 Pimpl 惯用法思想，只保存智能指针，具体类型在 cpp 中可见
    std::unique_ptr<ServiceRegistry> service_registry_;     // ZK 服务注册器
    std::unique_ptr<MessageCycle> message_cycle_;           // 网络事件循环引擎
};

} // namespace mooncrpc