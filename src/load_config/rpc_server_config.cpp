#include "rpc_server_config.h"
#include "log/log_manager.h"
#include <fstream>

namespace mooncrpc {

bool RpcServerConfig::InitRpcServerConfig(const std::string& config_file) {
    //1.打开文件
    std::ifstream file(config_file);
    if (!file.is_open()) {
        LOG_ERROR("打开服务端配置文件失败：{}", config_file);
        return false;
    }

    try {
        nlohmann::json config = nlohmann::json::parse(file);

        //2.加载自身的基础配置
        servers_name_prefix_ = config.value("servers_name_prefix", "mooncrpc");
        servers_ip_ = config.value("servers_ip", "0.0.0.0");
        servers_port_ = config.value("servers_port", 8989);
        timeout_ = config.value("timeout", 3000);
        max_connections_ = config.value("max_connections", 1000);

        //3.加载注册中心配置(委派给子模块)
        if (config.contains("register_config")) {
            std::string register_config_file = config.value("register_config", "../config/server.json");
            if (!service_registry_config_->InitRegistryConfig(register_config_file)) {
                LOG_ERROR("委托注册表配置初始化失败！");
                return false;
            }
        }

        //4.加载线程池配置(委派给子模块)
        if (config.contains("thread_pool")) {
            //将"thread_pool"对应的json对象传递给TheradPoolConfig处理
            if (!thread_pool_config_->InitThreadPoolConfig(config["thread_pool"])) {
                LOG_ERROR("委托线程池配置初始化失败！");
                return false;
            }
        }

        LOG_INFO("RPC 服务端配置的完整数据树已成功加载！");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("初始化服务端配置错误：{}", e.what());
        return false;
    }
}

}