#pragma once

#include <string>
#include <google/protobuf/message.h>
#include <unordered_map>

namespace mooncrpc {

/**
 * @brief RPC 客户端最终门面类
 */
class RpcClient {
public:
    // 构造函数：解析客户端配置并连接 Zookeeper 注册中心
    explicit RpcClient(const std::string& config_file);

    ~RpcClient();

    // 禁用拷贝与赋值
    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;

    /**
     * @brief 核心调用接口：发起同步 RPC 请求
     * @param service_name 目标服务名(eg. "UserService")
     * @param request 经过 Protobuf 序列化的请求对象
     * @param response 期待被填充的响应对象
     */
    bool Call(const std::string& service_name,
            const google::protobuf::Message& request,
            google::protobuf::Message& response);
    
private:
    uint32_t timeout_ms_;       // 网络超时时间
    uint32_t retry_times_;      // 失败重试次数

    // 客户端长连接池
    // Key: "IP:Port", Value: socket_fd
    std::unordered_map<std::string, int> connection_pool_;

    // 内部底层网络发送引擎
    bool SendAndReceive(const std::string& ip, uint16_t port,
                        const std::string& send_buf, std::string& recv_buf);
};

} //namespace mooncrpc