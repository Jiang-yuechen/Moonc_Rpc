//实现服务端总配置
#include "thread_pool_config.h"
#include "registry_config.h"
#include <string>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>

namespace mooncrpc {

//采用单例模式，保证全局只有一份服务端配置
class RpcServerConfig {
public:
    static RpcServerConfig& GetInstance() {
        static RpcServerConfig instance;
        return instance;
    }

    //初始化整个服务端配置(包括子配置)
    bool InitRpcServerConfig(const std::string& config_file);

    //获取基础配置参数
    const std::string& GetServersNamePrefix() const { return servers_name_prefix_; }
    const std::string& GetServersIp() const { return servers_ip_; }
    uint16_t GetServersPort() const { return servers_port_; }
    int GetTimeout() const { return timeout_; }
    int GetMaxConnections() const { return max_connections_; }

    //获取子配置模块的指针，供外部调用
    //.get()方法可以获取智能指针的裸指针类型
    const ServiceRegistryConfig* GetServiceRegistryConfig() const { return service_registry_config_.get(); }
    const ThreadPoolConfig* GetThreadPoolConfig() const { return thread_pool_config_.get(); }

    //禁用拷贝方法
    RpcServerConfig(const RpcServerConfig&) = delete;
    RpcServerConfig& operator= (const RpcServerConfig&) = delete;

private:
    //基础配置属性
    std::string servers_name_prefix_;
    std::string servers_ip_;
    uint16_t servers_port_ = 0;
    uint32_t timeout_ = 0;
    uint16_t max_connections_ = 0;

    //组合模式：独占智能指针管理子配置生命周期
    std::unique_ptr<ThreadPoolConfig> thread_pool_config_;
    std::unique_ptr<ServiceRegistryConfig> service_registry_config_;

    //私有构造函数，顺便实例化子模块的智能指针
    RpcServerConfig()
        : thread_pool_config_(std::make_unique<ThreadPoolConfig>()),
          service_registry_config_(std::make_unique<ServiceRegistryConfig>()) {}

};

}   //namespace mooncrpc