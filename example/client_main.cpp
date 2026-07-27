#include "core/rpc_client.h"
#include "message.pb.h"
#include "log/log_manager.h"
#include <iostream>

using namespace mooncrpc;

int main() {
    // 初始化日志系统
    if (!Logger::GetInstance().Init("./client_logs")) {
        return -1;
        std::cerr << "Initialize log_system failed!" << std::endl;
    }

    LOG_INFO("====== Starting MooncRPC Client ======");

    // 1. 初始化客户端通道
    RpcClient client("../config/rpc_client.json");

    // 2. 准备请求参数
    LoginRequest request;
    request.set_username("admin");
    request.set_password("123456");

    LoginResponse response;

    // 3. 发起远程过程调用 (RPC)
    CLIENT_INFO("Initiating RPC Call to [UserService]...");
    if (client.Call("UserService", request, response)) {
        
        // 4. 处理业务返回结果
        if (response.seccess()) {
            CLIENT_INFO("RPC Success! Received Token: {}", response.token());
        } else {
            CLIENT_INFO("RPC Success, but Business Logic Failed: {}", response.error_message());
        }
    } else {
        CLIENT_ERROR("RPC Framework Call Failed. Check network or Zookeeper.");
    }

    return 0;
}