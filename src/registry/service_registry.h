#pragma once

#include <string>
#include "../zookeeper/zk_client.h"     //引入封装的ZK驱动

namespace mooncrpc {

/**
 * @brief 服务端专属：分布式服务注册器
 * @details 负责将当前 RPC 服务端的机器实例信息注册到 ZooKeeper 注册中心
 */
class ServiceRegistry {
public:
    /**
     * @brief 构造函数：内部会自动发起与 ZooKeeper 集群的连接
     * @param zk_hosts ZooKeeper 服务器地址列表，例如 "127.0.0.1:2181"
     */
    explicit ServiceRegistry(const std::string& zk_hosts);

    /**
     * @brief 析构函数：利用 RAII 机制，在对象销毁时自动关闭 ZK 连接并注销服务
     */
    ~ServiceRegistry();
    
    /**
     * @brief 核心业务接口：注册单个服务实例到注册中心
     * @param service_name 服务名称，例如 "UserService" 或 "OrderService"
     * @param service_address 当前机器提供服务的真实网络地址，例如 "192.168.1.100:8989"
     * @return true 注册成功，false 注册失败
     */
    bool RegisterService(const std::string& service_name, const std::string& service_address);

    /**
     * @brief 查询当前注册中心是否在线
     * @return true 已连接，false 未连接
     */
    bool IsConnected();

private:
    ZkClient zk_client_;        //[组合模式]持有底层基础操作驱动
    static const std::string ROOT_PATH; //静态常量：整个mooncrpc框架在ZK中的命名空间根路径

    /**
     * @brief 递归创建持久化服务服务路径
     * @param path 需要校验并确保存在的路径
     */
    bool EnsurePath(const std::string &path);

    /**
     * @brief 创建具体的 ZooKeeper 节点
     * @param path 节点绝对路径
     * @param data 节点存储的数据内容
     * @param flags 节点类型控制（0为持久节点，ZOO_EPHEMERAL为临时节点）
     */
    bool CreateNode(const std::string &path, const std::string &data, int flags);
};

}