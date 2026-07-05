#include "rpc_server.h"
#include "../log/log_manager.h"
#include "../thread_pool/thread_pool.h"
#include "../service/service_manager.h"
#include "../network/message_cycle.h"
#include "../registry/service_registry.h"
#include "../load_config/rpc_server_config.h"

#include <stdexcept>

namespace mooncrpc {

RpcServer::RpcServer(const std::string& config_file) {
    // 1. 初始化配置中心 (自动解析 JSON)
    if (!RpcServerConfig::GetInstance().InitRpcServerConfig(config_file)) {
        LOG_FATAL("Failed to initialize RpcServerConfig from file: {}", config_file);
        throw std::runtime_error("RpcServer configuration failed");
    }

    auto& config = RpcServerConfig::GetInstance();
    ip_ = config.GetServersIp();
    port_ = config.GetServersPort();

    LOG_INFO("Initializing RpcServer on {}:{}", ip_, port_);

    // 2. 初始化高并发线程池，并注入到全局的 ServiceManager 中
    const ThreadPoolConfig* tp_config = config.GetThreadPoolConfig();
    if (tp_config != nullptr) {
        // 进行数据映射：将配置类的数据转移到底层参数结构体中
        ThreadPoolParams params;
        params.core_threads = tp_config->GetCoreThreads();
        params.max_threads = tp_config->GetMaxThreads();
        params.max_queue_size = tp_config->GetQueueSize();
        // 配置文件里的时间单位是s，线程池底层要求ms
        params.keep_alive_time = std::chrono::milliseconds(tp_config->GetKeepAliveTime() * 1000);

        auto thread_pool = std::make_shared<ThreadPool>(params);
        ServiceManager::GetInstance().Init(thread_pool);
    } else {
        // 配置文件中未定义线程池配置则使用默认参数
        auto thread_pool = std::make_shared<ThreadPool>(4);
        ServiceManager::GetInstance().Init(thread_pool);
        LOG_WARN("ThreadPool config missing, using default capacity (4 cores)");
    }

    // 3.初始化 Zookeeper 服务注册中心连接
    const ServiceRegistryConfig* reg_config = config.GetServiceRegistryConfig();
    if (reg_config != nullptr && reg_config->GetRegistryNodesSize() > 0) {
        std::string zk_host = reg_config->GetRegistryNodes()[0].address + ":" +
                            std::to_string(reg_config->GetRegistryNodes()[0].port);
        service_registry_ = std::make_unique<ServiceRegistry>(zk_host);
    } else {
        LOG_ERROR("Zookeeper registry configuration is missing or invalid!");
        throw std::runtime_error("Registry configuration failed");
    }

    // 4. 初始化网络 Reactor 事件循环引擎
    message_cycle_ = std::make_unique<MessageCycle>(ip_, port_);
    LOG_INFO("RpcServer engine assembly completed successfully.");
}

RpcServer::~RpcServer() {
    LOG_INFO("RpcServer is shutting down...");
    // 智能指针会自动销毁底层的 message_cycle_ 和 service_registry_
}

void RpcServer::RegisterService(std::shared_ptr<Service> service) {
    if (!service) {
        LOG_ERROR("Cannot register null service.");
        return;
    }

    // 1. 将服务挂载到本地的 ServiceManager 路由表中
    if (!ServiceManager::GetInstance().RegisterService(service)) {
        LOG_ERROR("Failed to register service locally: {}", service->GetServiceName());
        return;
    }

    // 2. 将服务信息发布到远程 Zookeeper 中，供客户端发现
    std::string service_name = service->GetServiceName();
    std::string service_address = ip_ + ":" + std::to_string(port_);

    if (service_registry_->RegisterService(service_name, service_address)) {
        LOG_INFO("Service [{}] successfully exposed at {}", service_name, service_address);
    } else {
        LOG_ERROR("Failed to publish service [{}] to Zookeeper", service_name);
    }
}

void RpcServer::Start() {
    LOG_INFO("RpcServer Event Loop starting. Ready to handle requests...");
    if (message_cycle_) {
        // 阻塞主线程，开始接客
        message_cycle_->Start();
    }
}

} //namespace mooncrpc
