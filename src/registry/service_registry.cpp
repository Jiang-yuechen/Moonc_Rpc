#include "service_registry.h"
#include "../log/log_manager.h"
#include <zookeeper/zookeeper.h>

/*
    * @brief 在具体实现中，将业务抽象（服务注册）无缝转化为对底层驱动的高效调用，并输出直观的中文日志。
*/

namespace mooncrpc {
    
//初始化静态常量：定义整个框架在 Zookeeper 中的根节点
const std::string ServiceRegistry::ROOT_PATH = "/mooncrpc";

//构造函数
ServiceRegistry::ServiceRegistry(const std::string &zk_hosts) {
    LOG_INFO("[服务注册器] 开始引导 Zookeeper 初始化连接...");
    //启动底层驱动，设置会话超时时间为30s
    if (!zk_client_.Start(zk_hosts, 30000)) {
        LOG_ERROR("[服务注册器] 连接 Zookeeper 发生错误！配置地址: {}", zk_hosts);
    }
}

//析构函数
ServiceRegistry::~ServiceRegistry() {
    //主动关闭底层的连接句柄
    zk_client_.Close();
    LOG_INFO("[服务注册器] 已成功释放底层连接资源。");
}

bool ServiceRegistry::IsConnected() {
    //通过全局根节点的存在性来校验当前网络是否连接
    return zk_client_.Exists("/");
}

bool ServiceRegistry::EnsurePath(const std::string& path) {
    //委派给底层驱动去执行多级目录递归创建逻辑
    return zk_client_.EnsurePath(path);
}

bool ServiceRegistry::CreateNode(const std::string& path, const std::string& data, int flags) {
    //委派给底层驱动去执行最基础的 ZK 节点写入
    return zk_client_.CreateNode(path, data, flags);
}

bool ServiceRegistry::RegisterService(const std::string& service_name, const std::string& service_address) {
    //1.安全性检查
    if (service_name.empty() || service_address.empty()) {
        LOG_ERROR("[服务注册失败] 服务名和服务地址参数非法, 不可为空！");
        return false;
    }

    //2.拼接该微服务器的专属持久化路径，eg：/mooncrpc/OrderService
    std::string service_path = ROOT_PATH + "/" + service_name;

    LOG_INFO("[服务注册] 正在确保该服务器的持久化目录结构存在: {}", service_path);
    if (!EnsurePath(service_path)) {
        LOG_ERROR("[服务注册失败] 无法在注册中心创建服务根目录: {}", service_path);
        return false;
    }

    //3.拼接当前物理机实例的唯一临时路径，eg：/mooncrpc/OrderService/192.168.0.101:8989
    std::string instance_path = service_path + "/" + service_address;
    
    LOG_INFO("[服务注册] 正在向注册中心挂载高可用节点: {}", instance_path);
    //创建临时节点flag，一旦服务器崩溃或出现故障，Zookeeper会自动移除这个节点
    return CreateNode(instance_path, service_address, ZOO_EPHEMERAL);
}

} //namespace mooncrpc