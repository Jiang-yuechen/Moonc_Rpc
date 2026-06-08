//服务注册配置
#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <nlohmann/json.hpp>

namespace mooncrpc {

//用来存储单台机器IP和端口的结构体
struct RegistryInfo{
    std::string address;
    uint16_t port;
};

//服务注册配置类专门读取即将注册到 ZooKeeper 里的服务节点信息
class ServiceRegistryConfig {
public:
    //默认构造与析构
    ServiceRegistryConfig() = default;
    ~ServiceRegistryConfig() = default;

    //解析注册中心配置
    //从文件初始化(从文件路径加载)
    bool InitRegistryConfig(const std::string& config_file);

    //Getters & Setters
    void SetServiceName(const std::string& name) { service_name_ = name; }
    const std::string& GetServiceName() const { return service_name_; }

    void SetServiceVersion(const std::string& version) { service_version_ = version; }
    const std::string& GetServiceVersion() const { return service_version_; }

    const std::vector<RegistryInfo>& GetRegistryNodes() const { return registry_nodes_; }
    size_t GetRegistryNodesSize() const { return registry_nodes_.size(); }

    //禁用拷贝和移动
    ServiceRegistryConfig(const ServiceRegistryConfig&) = delete;
    ServiceRegistryConfig& operator=(const ServiceRegistryConfig&) = delete;
    ServiceRegistryConfig(ServiceRegistryConfig&&) = delete;
    ServiceRegistryConfig& operator=(ServiceRegistryConfig&&) = delete;

private:
    std::vector<RegistryInfo> registry_nodes_;  //存储多个注册结点信息
    std::string service_name_;                  //服务名称
    std::string service_version_;               //服务版本
};

}