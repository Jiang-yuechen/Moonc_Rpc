#include "zk_conn_handler.h"
#include "../log/log_manager.h"
#include "conn_balancer/load_balancer.h"

namespace mooncrpc {

const std::string ZkConnHandler::ROOT_PATH = "/mooncrpc";

ZkConnHandler::ZkConnHandler() {
    //默认实例化随机负载均衡策略
    load_balancer_ = std::make_unique<RandonLoadBalancer>();
}

ZkConnHandler::~ZkConnHandler() {
    zk_client_.Close();
}

bool ZkConnHandler::Init(const std::string& zk_hosts) {
    LOG_INFO("[客户端服务发现] 正在连接 Zookeeper 注册中心...");
    if (!zk_client_.Start(zk_hosts)) {
        LOG_ERROR("[客户端服务发现] 连接 Zookeeper 失败！");
        return false;
    }
    return true;
}

std::string ZkConnHandler::GetAvailableServer(const std::string& service_name) {
    if (service_name.empty()) {
        LOG_ERROR("[服务发现失败] 请求的服务名为空！");
        return "";
    }

    //1.拼装要查询的服务节点路径
    std::string service_path = ROOT_PATH + "/" + service_name;

    //2.向 Zookeeper 拉取该路径下的所有在线子节点(IP 列表)
    std::vector<std::string> available_nodes = zk_client_.GetChildren(service_path);

    if (available_nodes.empty()) {
        LOG_ERROR("[服务发现失败] 服务[{}] 当前没有任何可用的在线节点！");
        return "";
    }

    LOG_DEBUG("服务 [{}] 共有 {} 个节点在线，准备及逆行负载均衡选择...", service_name, available_nodes.size());

    //3.使用负载均衡算法从列表中挑选一个节点返回
    std::string select_server = load_balancer_->Select(available_nodes);
    LOG_INFO("[负载均衡] 已为服务 [{}] 选取目标节点：{}", service_name, select_server);

    return select_server;
}

} //namespace mooncrpc