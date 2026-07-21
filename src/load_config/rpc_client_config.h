#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace mooncrpc {

class RpcClientConfig {
public:
    //单例模式
    static RpcClientConfig& GetInstance() {
        static RpcClientConfig instance;    //需要调用无参构造函数，在private中显示声明
        return instance;
    }

    //从文件中获取RPC客户端配置信息并初始化
    bool InitRpcClientConfig(const std::string& config_file);

    //Getters
    const std::string& GetZkHost() const { return zk_host_; }
    int GetZkPort() const { return zk_port_; }
    const std::string& GetZkNamespace() const { return zk_namespace_; }
    int GetTimeout() const { return timeout_ms_; }
    int GetRetryTimes() const { return retry_times_; }
    std::string GetLoadBalancerType() const { return lb_type_; }
    const std::unordered_map<std::string, int>& GetLoadBalancerWeights() const { return lb_weights_; }

    //Setters
    void SetZkHost(const std::string& host) { zk_host_ = host; }
    void SetZkPort(int port) { zk_port_ = port; }
    void SetZkNamespace(const std::string& ns) { zk_namespace_ = ns; }
    void SetTimeout(int timeout) { timeout_ms_ = timeout; }
    void SetRetryTimes(int retry) { retry_times_ = retry; }

    //禁用拷贝
    RpcClientConfig(const RpcClientConfig&) = delete;
    RpcClientConfig& operator=(const RpcClientConfig&) = delete;

private:
    std::string zk_host_;       //Zookeeper主机名
    int zk_port_ = 2181;        //Zookeeper默认端口2181
    std::string zk_namespace_;  //Zookeeper位置
    int timeout_ms_ = 3000;     //网络超时时间(ms)：客户端超过3000ms没动静就自动断开连接
    int retry_times_ = 3;       //客户端连接失败时最大重试次数

    std::string lb_type_ = "random";
    std::unordered_map<std::string, int> lb_weights_;

    //显式声明默认的无参构造函数，以能够无参构造单例
    //私有化为了防止外部通过RpcClientConfig config; 方式创建新对象
    RpcClientConfig() = default;
};
    

}   //namespace mooncrpc