/*
服务端行为：先向 ZK 注册 3 台 OrderService 的机器。
客户端行为：通过 ZkConnHandler 连续发起 5 次请求，看看负载均衡器是不是把请求随机分发给了这 3 台机器。
*/
#include "../src/registry/service_registry.h"
#include "../src/conn_balancer/zk_conn_handler.h"
#include "../src/log/log_manager.h"
#include <thread>
#include <chrono>

using namespace mooncrpc;

int main() {
    //启动日志服务
    if (!Logger::GetInstance().Init("./test_logs")) {
        return -1;
    }

    LOG_INFO("====== 开始运行 客户端服务发现与负载均衡 模块测试 ======");

    std::string zk_addr = "127.0.0.1:2181";
    std::string test_service = "OrderService";

    //阶段1：模拟服务端启动集群（注册三台机器）
    LOG_INFO(">>> 阶段 1: 服务端集群启动中...");
    ServiceRegistry registry(zk_addr);
    registry.RegisterService(test_service, "192.168.1.10:8001");
    registry.RegisterService(test_service, "192.168.1.20:8002");
    registry.RegisterService(test_service, "192.168.1.30:8003");

    //等待 ZK 同步节点树结构
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    //阶段2：模拟客户端请求与服务发现
    LOG_INFO(">>> 阶段2：客户端初始化服务发现组件...");
    auto& conn_handler = ZkConnHandler::GetInstance();
    if (!conn_handler.Init(zk_addr)) {
        LOG_ERROR("客户端服务发现组件初始化失败！");
        return -1;
    }

    LOG_INFO(">>> 客户端连续发起 5 次服务寻址请求，验证负载均衡：");
    for (int i = 1; i <= 5; i++) {
        std::string target_ip = conn_handler.GetAvailableServer(test_service);
        if (!target_ip.empty()) {
            LOG_INFO("  [第 {} 次请求] RPC 客户端将被路由到目标机器：{}", i, target_ip);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    //阶段3：测试故障剔除机制
    LOG_INFO(">>> 阶段3：故障测试 -> 查询一个不存在的服务");
    conn_handler.GetAvailableServer("NotExistService");

    LOG_INFO("===== 测试结束 =====");

    return 0;
}