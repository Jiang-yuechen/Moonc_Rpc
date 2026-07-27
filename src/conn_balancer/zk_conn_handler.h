//这是一个单例类，全局负责维护客户端与 ZK 的连接，并对外提供找机器的接口
#pragma once

#include "zookeeper/zk_client.h"
#include "conn_balancer/load_balancer.h"
#include <memory>
#include <chrono>
#include <unordered_map>

namespace mooncrpc {

//客户端专属：Zookeeper 连接与服务器发现管理器
class ZkConnHandler {
public:
    //单例模式
    static ZkConnHandler& GetInstance() {
        static ZkConnHandler instance;
        return instance;
    }

    //初始化客户端 ZK 连接
    //zk_hosts ZK集群地址
    bool Init(const std::string& zk_hosts);

    //核心接口：服务发现与负载均衡
    //service_name 需要调用的服务名(eg. "OrderService")
    //return 经过负载均衡后挑选的机器地址 (eg. "192.168.0.101:8989")
    std::string GetAvailableServer(const std::string& service_name);

    //禁用拷贝和赋值(单例模式)
    ZkConnHandler(const ZkConnHandler&) = delete;
    ZkConnHandler& operator=(const ZkConnHandler&) = delete;

private:
    ZkClient zk_client_;            //组合 ZK 底层驱动
    //std::unique_ptr<LoadBalancer> load_balancer_;   //组合负载均衡策略
    static const std::string ROOT_PATH;             //根路径 "/moonrpc"

    // v2：增加本地缓存机制
    struct CacheItem {
        std::vector<std::string> nodes;                         // 可用节点
        std::chrono::steady_clock::time_point last_update;      // 最近更新时间
    };

    std::unordered_map<std::string, CacheItem> local_cache_;    // 缓存表
    std::mutex cache_mutex_;            // 保护缓存表的并发安全锁
    const int TTL_SECONDS = 10;         // 缓存有效时间设为 10s

    ZkConnHandler();
    ~ZkConnHandler();
};

}