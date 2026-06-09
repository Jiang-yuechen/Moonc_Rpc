#include "registry_config.h"
#include "../log/log_manager.h"
#include <fstream>      //主要用来处理“文件内容”的读和写(打开文件读(ifstream), 打开文件写(ofstream))

namespace mooncrpc {

//从文件获取信息进行初始化服务端注册配置
bool ServiceRegistryConfig::InitRegistryConfig(const std::string& config_file) {
    //1.打开文件
    std::ifstream file(config_file);    //ifstream采取RAII机制
    if (!file.is_open()) {
        LOG_ERROR("打开服务端配置文件失败：{}", config_file);
        return false;
    }

    try {
        //2.将file文件内容解析为JSON对象，nlohmann::json数据类型类似于std::map 或 std::string
        nlohmann::json config = nlohmann::json::parse(file);

        //3.加载基础信息
        std::string service_name = config.value("service_name", "");
        if (service_name.empty()) {
            LOG_ERROR("配置文件({})中的服务名为空！");
            return false;
        }
        //设置服务名与版本
        SetServiceName(service_name);
        std::string service_version = config.value("service_version", "1.0.0");
        SetServiceVersion(service_version);

        //4.读取结点数组(JSON Array)
        if (!config.contains("registry_nodes")) {
            LOG_ERROR("{}中没有找到注册结点信息！");
            return false;
        }

        std::vector<RegistryInfo> tmp_registry_nodes;    //{ address, prot }
        //遍历JSON数组，提取address 和 port
        for (const auto& node : config["registry_nodes"]) {
            if (!node.contains("address") || !node.contains("port")) {
                LOG_ERROR("结点信息格式不正确！");
                return false;
            }
            RegistryInfo tmp_node;
            tmp_node.address = node["address"];
            tmp_node.port = node["port"];
            tmp_registry_nodes.push_back(tmp_node);     //注册结点信息存入数组
        }

        //5.更新成员变量
        registry_nodes_ = std::move(tmp_registry_nodes);    //使用移动语义避免不必要的深拷贝
        LOG_INFO("服务端注册配置结点信息加载成功！结点数：{}", registry_nodes_.size());
        return true;
    } catch (const nlohmann::json::exception& e) {
        LOG_ERROR("{}JSON解析错误:{}！", config_file, e.what());
        return false;
    }
}

}       //namespace mooncrpc
