#pragma once

#ifndef THREADED
#define THREADED  //必须在 include 之前定义
#endif

#include <string>
#include <vector>
#include <zookeeper/zookeeper.h>
#include <semaphore.h>  //用于阻塞等待异步的ZK连接建立

namespace mooncrpc {

class ZkClient {
public:
    //构造函数与析构函数
    ZkClient();
    ~ZkClient();

    //启动连接ZK服务器
    //zk_hosts: "127.0.0.1:2181" 格式
    //timeout_ms：会话超时时间(ms)
    bool Start(const std::string& zk_hosts, int timeout_ms = 30000);

    //关闭连接
    void Close();

    /*================= 节点注册模块功能 ===============*/

    // 创建节点 (可以是持久节点或临时节点)
    // path: 节点路径，例如 "/mooncrpc/UserService"
    // data: 节点存放的数据，例如 "127.0.0.1:8989"
    // flags: 0 表示持久节点，ZOO_EPHEMERAL 表示临时节点
    bool CreateNode(const std::string& path, const std::string& data, int flags = 0);

    // 确保持久化路径存在（如果父节点不存在则自动递归创建）
    // RPC 框架中，服务名通常是持久节点，具体的机器 IP 是挂在它下面的临时节点
    bool EnsurePath(const std::string& path);

    //删除节点
    bool DeleteNode(const std::string& path);

    /*============ 节点管理模块 =============*/

    //获取某个路径下的所有子节点(服务发现的核心)
    std::vector<std::string> GetChildren(const std::string& path);

    //获取某个节点里存储的数据
    std::string GetData(const std::string& path);

    //判断节点是否存在
    bool Exists(const std::string& path);

private:
    zhandle_t* zk_handle_;  //Zookeeper客户端的底层操作句柄，需要基于这个句柄操作Zookeeper
    bool is_connected_;     //标记是否成功连接    

    //ZK异步连接建立时的全局回调函数
    static void GlobalWatcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);
};

}       //namespace mooncrpc