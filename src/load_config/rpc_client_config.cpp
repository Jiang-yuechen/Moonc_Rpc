#include "rpc_client_config.h"
#include "../log/log_manager.h"
#include <fstream>

namespace mooncrpc {

bool RpcClientConfig::InitRpcClientConfig(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        LOG_ERROR("打开客户端配置文件失败：{}", config_file);
        return false;
    }

    try {
        nlohmann::json config = nlohmann::json::parse(file);

        if (config.contains("load_balancer")) {
            auto lb_config = config["load_balancer"];
            lb_type_ = lb_config.value("type", "random");
            
            if (lb_config.contains("weights")) {
                for (auto& el : lb_config["weights"].items()) {
                    lb_weights_[el.key()] = el.value().get<int>();
                }
            }
        }
        //使用 .value() 提供安全默认值
        SetZkHost(config.value("zk_host", "127.0.0.1"));
        SetZkPort(config.value("zk_port", 2181));
        SetZkNamespace(config.value("zk_namespace", "/mooncrpc"));
        SetTimeout(config.value("timeout_ms", 3000));
        SetRetryTimes(config.value("retry_times", 3));

        LOG_INFO("Rpc客户端配置加载成功: {}", config_file);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("客户端初始化发生错误: {}", e.what());
        return false;
    }
}

}       //namespace mooncrpc