//模拟多台服务器并发上线注册同一个微服务的情景，验证整个注册器的功能。

#include "../src/log/log_manager.h"
#include "../src/registry/service_registry.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace mooncrpc;

int main() {
    //1.初始化日志系统
    if (!Logger::GetInstance().Init("./test_logs")) {
        std::cerr << "日志初始化失败！" << std::endl;
        return -1;
    }

    LOG_INFO("======= 开始 ServiceRegistry 模块业务测试 =======");

    //2.实例化服务注册器，连接本地 Zookeeper 服务
    ServiceRegistry server_registry("127.0.0.1:2181");

    //3.模拟分布式场景
    std::string rpc_service = "OrderService";
    std::string node_server_1 = "192.168.0.101:8989";
    std::string node_server_2 = "192.168.0.102:8989";

    LOG_INFO("正在注册 1 号服务器实例...");
    if (server_registry.RegisterService(rpc_service, node_server_1)) {
        LOG_INFO("[测试反馈] 1 号物理节点成功上线！");
    }
    LOG_INFO("正在注册 2 号服务器实例...");
    if (server_registry.RegisterService(rpc_service, node_server_2)) {
        LOG_INFO("[测试反馈] 2 号物理节点成功上线！");
    }

    // 4. 保持程序活着，方便在 Ubuntu 命令行里验证结果
    LOG_INFO("[测试提示]所有集群服务已成功挂载！程序即将休眠 5 秒。");
    LOG_INFO("你可以立刻打开另外一个终端执行: zkCli.sh -server 127.0.0.1:2181");
    LOG_INFO("运行命令: ls /mooncrpc/OrderService 即可看到这两台在线机器！");

    std::this_thread::sleep_for(std::chrono::seconds(10));
    LOG_INFO("====== 测试结束，程序退出后临时节点将自动销毁 ======");
    return 0;
}