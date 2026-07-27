#include "zk_conn_handler.h"
#include "log/log_manager.h"
#include "conn_balancer/load_balancer.h"

namespace mooncrpc {

const std::string ZkConnHandler::ROOT_PATH = "/mooncrpc";

ZkConnHandler::ZkConnHandler() {
    //默认实例化随机负载均衡策略
    //load_balancer_ = std::make_unique<RandomLoadBalancer>();
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

// v2：新增缓存拦截
std::string ZkConnHandler::GetAvailableServer(const std::string& service_name) {
    if (service_name.empty()) {
        LOG_ERROR("[服务发现失败] 请求的服务名为空！");
        return "";
    }

    //1.拼装要查询的服务节点路径
    std::string service_path = ROOT_PATH + "/" + service_name;

    
    std::vector<std::string> available_nodes;       // 用于存储可用节点

    // 2. v2：先查本地缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = local_cache_.find(service_name);
        if (it != local_cache_.end()) {
            // 计算当前时间与缓存时间差值，判断缓存是否过期
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_update).count();

            // 缓存未过期且里面有节点数据则直接使用
            if (duration < TTL_SECONDS && !it->second.nodes.empty()) {
                available_nodes = it->second.nodes; 
            }
        }
    }

    // 3. 如果缓存未命中或者已过期，再去发起真实的 ZK 网络查询
    if (available_nodes.empty()) {
        // 向 Zookeeper 拉取该路径下的所有在线子节点(IP 列表)
        available_nodes = zk_client_.GetChildren(service_path);

        if (available_nodes.empty()) {
            LOG_ERROR("[服务发现失败] 服务[{}] 当前没有任何可用的在线节点！");
            return "";
        }

        // 查询成功后，立刻写入本地缓存并更新时间戳
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            local_cache_[service_name] = {available_nodes, std::chrono::steady_clock::now()};
        }
        // 为了不影响终端输出，这里使用 DEBUG 级别
        CLIENT_DEBUG("[服务发现] 缓存未命中或已过期，从 ZK 拉取最新节点列表并更新本地缓存。");
    }

    CLIENT_DEBUG("服务 [{}] 共有 {} 个节点在线，准备进行负载均衡选择...", service_name, available_nodes.size());

    // 3.使用负载均衡算法从列表中挑选一个节点返回
    // std::string select_server = load_balancer_->select(available_nodes);
    std::string select_server = LoadBalancer::selectServer(available_nodes);
    CLIENT_INFO("[负载均衡] 已为服务 [{}] 选取目标节点：{}", service_name, select_server);

    return select_server;
}

} //namespace mooncrpc