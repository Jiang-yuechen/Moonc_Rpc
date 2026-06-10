/*
测试：
1.是否能连上本机的 ZK。
2.能否创建父节点。
3.能否在父节点下创建子节点（模拟 RPC Server 注册上线）。
4.能否查出子节点列表（模拟 RPC Client 发现服务）。
*/
#include "../src/log/log_manager.h"
#include "../src/zookeeper/zk_client.h"
#include <thread>
#include <zookeeper/zookeeper.h>

using namespace mooncrpc;

int main() {
    //1.初始化日志
    if (!Logger::GetInstance().Init("./test_logs")) {
        std::cerr << "初始化日志系统失败！" << std::endl;
        return -1;
    }

    LOG_INFO("======== 开始 Zookeeper 模块测试 ========");

    ZkClient zk_client;

    //2.启动连接(需要虚拟机中的zookeeper在2181端口已经启动)
    //启动Zookeeper服务器：sudo /usr/share/zookeeper/bin/zkServer.sh start
    //查看Zookeeper服务器状态：sudo /usr/share/zookeeper/bin/zkServer.sh status
    //停止Zookeeper服务：sudo /usr/share/zookeeper/bin/zkServer.sh stop
    if (!zk_client.Start("127.0.0.1:2181")) {
        LOG_ERROR("ZK 连接失败，测试终止！请检查 Zookeeper 是否启动！");
        return -1;
    }

    //3.测试：递归创建父路径（模拟 RPC 框架的根目录及服务目录）
    std::string service_path = "/mooncrpc/TestService";
    LOG_INFO("--- 正在确保服务父路径存在 ---");
    zk_client.EnsurePath(service_path);

    //4.测试：注册临时节点（模拟某台服务器 192.168.1.100:8888 启动并注册）
    std::string instance_path = service_path + "/192.168.1.100:8888";
    LOG_INFO("--- 正在注册临时服务节点 ---");
    zk_client.CreateNode(instance_path, "Server Data", ZOO_EPHEMERAL);

    //5.测试：服务发现（模拟客户端拉取在线的机器列表）
    LOG_INFO("--- 获取服务在线列表 ---");
    std::vector<std::string> nodes = zk_client.GetChildren(service_path);
    LOG_INFO("发现 [{}] 服务共有 {} 台在线机器：", "TestService", nodes.size());
    for (const auto& node : nodes) {
        LOG_INFO(" -> 发现可用节点 IP: {}", node);
    }

    //6.启动Zookeeper客户端并连接到服务器：sudo /usr/share/zookeeper/bin/zkCli.sh -server localhost:2181
    //等待几秒，可以在终端里用 ls /mooncrpc/TestService 看，当前节点是存在的
    LOG_INFO("休眠 10 秒, 现在可以通过 ls /mooncrpc/TestService 查看节点...");
    std::this_thread::sleep_for(std::chrono::seconds(10));

    //7.删除节点（临时节点不手动删也可以，ZK 服务器会自动删除）
    LOG_INFO("--- 清理测试节点 ---");
    zk_client.DeleteNode(instance_path);

    LOG_INFO("======== Zookeeper 模块测试结束 =======");
    return 0;
}