#include "../src/load_config/rpc_server_config.h"
#include "../src/log/log_manager.h"
#include <iostream>

using namespace mooncrpc;
using json = nlohmann::json;

int main() {
    //1.初始化日志模块
    if (!Logger::GetInstance().Init("./test_logs")) {
        std::cerr << "初始化日志失败！" << std::endl;
        return -1;
    }

    LOG_INFO("======开始RPC服务端配置组合测试======");
    //2.获取单例并加载真实文件
    auto& server_config = RpcServerConfig::GetInstance();

    if (server_config.InitRpcServerConfig("../config/rpc_server.json")) {   //相对目录是build/ 开始
        LOG_INFO("---基础服务端配置---");
        LOG_INFO("Prefix: {}", server_config.GetServersNamePrefix());
        LOG_INFO("IP:Port: {}:{}", server_config.GetServersIp(), server_config.GetServersPort());

        LOG_INFO("---委派解析的线程池配置---");
        auto* thread_pool_config = server_config.GetThreadPoolConfig();
        LOG_INFO("Core Threads: {}", thread_pool_config->GetCoreThreads());

        LOG_INFO("---委派解析的注册中心配置---");
        auto* registry_config = server_config.GetServiceRegistryConfig();
        LOG_INFO("Service Name: {}", registry_config->GetServiceName());
        LOG_INFO("Node Count: {}", registry_config->GetRegistryNodesSize());
        for (const auto& node :registry_config->GetRegistryNodes()) {   //获取注册节点信息结构体数组
            LOG_INFO(" -> Node IP:prot : {}:{}", node.address, node.port);
        }
    } else {
        LOG_ERROR("解析RPC服务端配置失败！");
    }

    // //3.异常测试
    // LOG_INFO("======测试异常配置======");
    // json invalid_config = {
    //     { "core_threads", 0}
    // };
    // ThreadPoolConfig invalid_cr_config;
    // if (!invalid_cr_config.InitThreadPoolConfig(invalid_config)) {
    //     LOG_INFO("已重新调整为合理配置");
    // } else {
    //     LOG_ERROR("重新调整配置数据失败！");
    // }

    LOG_INFO("===RPC服务端配置数据测试结束===");
    return 0;
}