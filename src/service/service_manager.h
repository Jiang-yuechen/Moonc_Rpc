//全局唯一路由器
#pragma once

#include "thread_pool/thread_pool.h"
#include "service.h"
#include <string>
#include <unordered_map>
#include <memory.h>
#include <mutex>
#include <functional>

namespace mooncrpc {

/**
 * @brief 全局服务管理器 (单例模式)
 * @details 充当 RPC 框架的“中央分发器”
 * 负责管理所有已注册的服务，并将收到的网络请求同步或异步派发给具体的服务实例
 * 
 *职责：
 * 1. 服务注册：RegisterService()
 * 2. 服务查找：GetService()
 * 3. 请求分发：HandleRpcRequestSync() / HandleRpcRequestAsync()
 * 4. 线程池管理：集成 ThreadPoolSingleton 实现异步处理
 *
 * 线程安全：
 * - 使用 std::mutex 保护 service_map_ 映射表
 * - 所有公开方法都加锁
 */
class ServiceManager {
public:
    //单例模式
    static ServiceManager& GetInstance() {
        static ServiceManager instance;
        return instance;
    }

    //初始化管理器：注入底层线程池引擎
    void Init(std::shared_ptr<ThreadPool> thread_pool);

    //注册新服务到路由表中
    bool RegisterService(std::shared_ptr<Service> service);

    //从路由表中查找指定的服务
    std::shared_ptr<Service> GetService(const std::string& service_name);

    //同步请求处理：阻塞当前线程直到业务处理完成
    void HandleRpcRequestSync(const std::string& service_name, const RpcMessage& request, RpcMessage& response);

    //异步处理请求：将任务丢入线程池，处理完成后触发 callback 回调函数发回给网络层
    void HandleRpcRequestAsync(const std::string& service_name,
                                const RpcMessage& request,
                                std::function<void(const RpcMessage&)> callback);
    
    //获取底层线程池的统计信息
    ThreadPoolStats GetThreadPoolStats() const;

    //单例模式手法：禁用拷贝和赋值
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;

private:
    ServiceManager() = default;
    ~ServiceManager() = default;

    //核心路由表：服务名 -> 服务实例指针
    std::unordered_map<std::string, std::shared_ptr<Service>> service_map_;

    //保护路由表的读写锁 (支持高并发注册于查找)
    mutable std::mutex mutex_;
    
    //底层异步处理引擎
    std::shared_ptr<ThreadPool> thread_pool_;
};

}
