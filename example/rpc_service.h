#pragma once

#include "service/service.h"
#include "message.pb.h"
#include "serializer/serializer_manager.h"
#include "protocol/rpc_error_code.h"

namespace mooncrpc{

/**
 * @brief 用户服务提供者类
 * @details 继承自 Service 基类，负责处理路由到 "UserService" 的所有 RPC 请求。
 */
class UserServiceImpl : public Service {
    /**
     * @brief 获取当前服务的路由名称
     * @return std::string 服务的唯一标识符
     * @note 返回值必须与 Protobuf 中定义的 service 名称保持严格一致，用于 ServiceManager 的哈希路由。
     */
    std::string GetServiceName() const override {
        return "UserService";
    }

    /**
     * @brief RPC 请求的统一处理入口
     * @param request 客户端发来的请求包 (包含包头元数据及二进制 body)
     * @param response 准备返回给客户端的响应包 (由本函数负责填充业务 body 和状态码)
     * @details
     * 执行流水线:
     * 1. 根据约定的协议 (Protobuf) 反序列化 request.body。
     * 2. 执行核心业务逻辑 (如校验账户密码)。
     * 3. 将业务响应对象重新序列化并赋值给 response.body。
     * 4. 设置框架级 RPC 错误码。
     */
    void HandleRpcRequest(const RpcMessage& request, RpcMessage& response) override {
        LOG_INFO("UserServiceImpl received a remote request.");

        // 1. 反序列化请求
        LoginRequest login_req;
        if (!SerializerManager::Deserialize(request.body, login_req, SerializeType::PROTOBUF)) {
            response.header.error_code = static_cast<uint32_t>(RpcErrorCode::DESERIALIZE_FAILED);
            return;
        }

        // 2. 执行真正的业务逻辑
        LoginResponse login_res;
        if (login_req.username() == "admin" && login_req.password() == "123456") {
            login_res.set_seccess(true);
            login_res.set_token("Moonc_Super_Token_999");
            LOG_INFO("User [{}] login success.", login_req.username());
        } else {
            login_res.set_seccess(false);
        }

        // 3. 业务数据序列化并封装回执包
        response.body = SerializerManager::Serialize(login_res, SerializeType::PROTOBUF);

        response.header.error_code = static_cast<uint32_t>(RpcErrorCode::SUCCESS);
    }
};

} // namespace mooncrpc