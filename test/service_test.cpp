#include "../src/log/log_manager.h"
#include "../src/thread_pool/thread_pool.h"
#include "../src/service/service_manager.h"
#include <iostream>
#include <thread>

using namespace mooncrpc;

//eg.用户自定义的业务服务实现
class UserService : public Service {
public:
    std::string GetServiceName() const override {
        return "UserService";
    }

    void HandleRpcRequest(const RpcMessage& request, RpcMessage& response) override {
        LOG_INFO("UserService is processing request... Payload: {}", request.body);

        //模拟业务耗时处理
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        //组装处理结果
        response.header.error_code = 0;
        response.body = "Login SUCCESS! Token: moonc_token_408";
    };
};

int main() {
    //加载日志
    if (!Logger::GetInstance().Init("./test_logs")) {
        return -1;
    }

    LOG_INFO("=== Starting Local Service Registry & Dispatcher Test ======");

    //1. 初始化引擎：创建线程池并交给 ServiceManager 管理
    auto thread_pool = std::make_shared<ThreadPool>(4);
    auto& dispatcher = ServiceManager::GetInstance();
    dispatcher.Init(thread_pool);

    //2. 服务注册
    auto user_service = std::make_shared<UserService>();
    dispatcher.RegisterService(user_service);

    //3. 构造一个模拟的客户端请求包
    RpcMessage request;
    request.header.seq_id = 8888;
    request.body = "username:admin, password:123";

    //4. 同步请求测试
    LOG_INFO("--- Phase 1: Sync Request Dispatching ---");
    RpcMessage sync_response;
    dispatcher.HandleRpcRequestSync("UserService", request, sync_response);
    LOG_INFO("Sync Dispatch SUCCESS. Response SeqId: {}, Body:{}",
            sync_response.header.seq_id, sync_response.body);

    // 5. 异步请求测试 (交给线程池处理，主线程继续往下走)
    LOG_INFO("--- Phase 2: Async Request Dispatching ---");
    
    // 定义一个回调函数，模拟网络层收到业务结果后准备发送的数据
    auto mock_network_send_callback = [](const RpcMessage& response) {
        LOG_INFO("[Network Layer Callback] Async processing done! Preparing to send response over TCP...");
        LOG_INFO("[Network Layer Callback] Final Result -> Code: {}, Body: {}", 
                 response.header.error_code, response.body);
    };

    dispatcher.HandleRpcRequestAsync("UserService", request, mock_network_send_callback);

    // 主线程干点别的事，并打印线程池状态，证明异步生效
    LOG_INFO("Main thread continues immediately without blocking...");
    auto stats = dispatcher.GetThreadPoolStats();
    LOG_INFO("Current Active Threads processing background tasks: {}", stats.active_threads);

    // 等待异步任务执行完成
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 6. 异常路由测试 (访问未注册的服务)
    LOG_INFO("--- Phase 3: Unregistered Service Dispatching ---");
    RpcMessage error_response;
    dispatcher.HandleRpcRequestSync("OrderService", request, error_response);
    LOG_INFO("Expected Error Caught! ErrorCode: {}, Message: {}", 
             error_response.header.error_code, error_response.body);

    // 彻底关闭底层线程池
    thread_pool->Shutdown();
    LOG_INFO("====== Local Service Registry Test Completed Successfully ======");
    return 0;
}