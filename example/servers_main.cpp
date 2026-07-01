#include "core/rpc_server.h"
#include "rpc_service.h"
#include "log/log_manager.h"

using namespace mooncrpc;

int main() {
    // 初始化日志系统
    if (!Logger::GetInstance().Init("./server_logs")) {
        return -1;
    }

    LOG_INFO("====== Starting MooncRPC Server ======");

    try {
        // 1. 加载配置并初始化引擎
        RpcServer server("../config/rpc_server.json");

        // 2. 注册业务服务
        server.RegisterService(std::make_shared<UserServiceImpl>());

        // 3. 启动死循环接客 (阻塞)
        server.Start();

    } catch (const std::exception& e) {
        LOG_FATAL("Server crashed with exception: {}", e.what());
    }

    return 0;
}