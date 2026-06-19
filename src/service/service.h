/* 定义抽象接口 */
#pragma once
#include "../protocol/rpc_protocol.h"
#include <string>

namespace mooncrpc {

/**
 * @brief 抽象服务接口
 * @details 所有具体的 RPC 服务实现类都必须继承此接口。
 * 它定义了服务的基本规范：叫什么名字，以及如何处理请求。 */
class Service {
public:
    //虚析构函数：确保通过基类指针删除派生类对象时正确析构
    virtual ~Service() = default;

    /**
     * @brief 纯虚函数：获取当前服务的唯一名称 (eg. "UserService")
     * @return 服务名称字符串
     * 
     * 服务名用于：
     * 1.服务注册时作为 key 存入 ServiceManager
     * 2.客户端调用时通过服务名查找对应服务
     * 3.服务发现和负载均衡
     */
    virtual std::string GetServiceName() const = 0;

    /**
     * @brief 纯虚函数：核心业务处理接口
     * @param request 包含了解包后的请求数据
     * @param response 用于向其中填充处理结果和错误码
     */
    virtual void HandleRpcRequest(const RpcMessage& request, RpcMessage& response) = 0;
};

} //namespace mooncrpc