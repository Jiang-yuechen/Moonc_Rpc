#include "zk_client.h"
#include "../log/log_manager.h"
#include <iostream>
#include <zookeeper/zookeeper.h>
#include <zookeeper/zookeeper.jute.h>

#define MAXSIZE 1024

namespace mooncrpc {

//构造函数：初始化句柄
ZkClient::ZkClient() : zk_handle_(nullptr), is_connected_(false) {}

//析构函数：利用RAII机制子哦对那个释放连接，防止资源泄漏
ZkClient::~ZkClient() {
    Close();
}

//全局的 Watcher 回调函数
//由于 zookeeper_init 是异步的，它会立即返回，所以我们需要在回调里判断是否真正脸上了
void ZkClient::GlobalWatcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx) {
    if (type == ZOO_SESSION_EVENT) {    //只关心会话级别的事件（也就是连接状态的变化）
        //使用信号量来通知主线程连接结果
        sem_t* sem = static_cast<sem_t*>(watcherCtx);    //将 void* 类型强转成信号量指针 sem_t*
        if (state == ZOO_CONNECTED_STATE) {    //会话事件有很多种（连接中、连接断开、会话过期等），只关心已连接事件
            sem_post(sem);  //状态变为已连接，释放信号量，唤醒主线程（V操作）
        }
    }
}

//启动连接 ZK 服务器
bool ZkClient::Start(const std::string& zk_hosts, int timeout_ms) {
    //隐藏 ZK 底层繁琐的 Debug 日志
    zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);

    sem_t sem;
    sem_init(&sem, 0, 0);   //初始化信号量

    //发起异步连接请求，将信号量指针作为上下文(watcherCtx)传给回调函数
    zk_handle_ = zookeeper_init(zk_hosts.c_str(), GlobalWatcher, timeout_ms, nullptr, &sem, 0);

    if (zk_handle_ == nullptr) {
        LOG_ERROR("Zookeeper 初始化句柄失败！配置地址：{}", zk_hosts);
        return false;
    }

    LOG_INFO("正在尝试连接到 Zookeeper 服务器：{}...", zk_hosts);

    //阻塞等待，直到 GlobalWatcher 内部调用 sem_post 唤醒
    sem_wait(&sem);
    sem_destroy(&sem);

    //连接成功
    is_connected_ = true;
    LOG_INFO("成功连接到 Zookeeper 服务器！");
    return true;
}

//关闭 Zookeeper 连接
void ZkClient::Close() {
    if (zk_handle_ != nullptr) {
        zookeeper_close(zk_handle_);
        zk_handle_ = nullptr;
        is_connected_ = false;
        LOG_INFO("Zookeeper 连接已关闭");
    }
}

//在 Zookeeper 服务器上创建节点并存储数据
bool ZkClient::CreateNode(const std::string& path, const std::string& data, int flags) {
    if (!is_connected_) {
        LOG_ERROR("Zookeeper 未连接，创建节点失败！");
        return false;
    }

    //Zookeeper 如果使用 ZOO_SEQUENCE 标志创建顺序节点时会自动在节点名后面加编号
    //这会导致最终生成的名字和传进去的path不一致，所以要用一个buffer存储创建后真正的名字
    char path_buffer[MAXSIZE];  
    int buffer_len = sizeof(path_buffer);

    //ZOO_OPEN_ACL_UNSAFE 表示节点没有任何权限，任何人都可以读写
    int ret = zoo_create(zk_handle_,
                        path.c_str(),    //想创建的节点路径
                        data.c_str(),    //要存进去的数据，一般是服务端真实ip与端口
                        data.size(),     //要存的数据长度
                        &ZOO_OPEN_ACL_UNSAFE,    //表示节点没有任何权限，任何人都可以读写
                        flags,    //节点类型：通常填 ZOO_EPHEMERAL（临时节点，客户端一断开节点就自动消失，非常适合做服务注册）或者 0（持久节点）
                        path_buffer,
                        buffer_len);
    
    if (ret == ZOK) {
        LOG_INFO("成功创建 Zookeeper 节点：{} [{}]", path,
            (flags == ZOO_EPHEMERAL ? "临时节点" : "持久节点"));
            return true;
    } else if (ret == ZNODEEXISTS) {
        LOG_WARN("Zookeeper 节点已经存在：{}", path);
        return true;
    } else {
        LOG_ERROR("创建 Zookeeper 节点失败：{}, 错误码：{}", path, ret);
        return false;
    }
}

//递归创建持久化路径(类似 mkdir -p)
bool ZkClient::EnsurePath(const std::string& path) {
    if (path.empty() || path == "/") {
        return true;
    }
    
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos && pos > 0) {
        std::string parent = path.substr(0, pos);
        //递归确保父路径存在
        if (!EnsurePath(parent)) {
            return false;
        }
    }

    if (Exists(path)) {
        return true;
    }

    //创建当前层级的持久节点
    return CreateNode(path, "", 0);
}

//判断路径是否存在
bool ZkClient::Exists(const std::string& path){
    if (!is_connected_) {
        return false;
    }
    
    int ret = zoo_exists(zk_handle_, path.c_str(), 0, nullptr);
    return (ret == ZOK);
}

//删除节点
bool ZkClient::DeleteNode(const std::string& path) {
    if (!is_connected_) {
        return false;
    }

    int ret = zoo_delete(zk_handle_, path.c_str(), -1);    //-1表示忽略版本校验，直接删除
    if (ret == ZOK) {
        LOG_INFO("成功删除 Zookeeper 节点：{}", path);
        return true;
    }
    LOG_ERROR("删除 Zookeeper 节点失败：{}, 错误码:{}", path, ret);
    return false;
}

//获取子节点列表(客户端服务发现使用)
std::vector<std::string> ZkClient::GetChildren(const std::string& path) {
    std::vector<std::string> children;
    if (!is_connected_) {
        return children;
    }

    struct String_vector child_vec;
    int ret = zoo_get_children(zk_handle_, path.c_str(), 0, &child_vec);

    if (ret == ZOK) {   //成功获取孩子节点
        for (int i = 0; i < child_vec.count; i++) {
            children.push_back(child_vec.data[i]);
        }

        //C语言API分配内存需要手动释放，否则会内存泄漏
        deallocate_String_vector(&child_vec);
    } else {
        LOG_ERROR("获取子节点失败:{}, 错误码:{}", path, ret);
    }

    return children;
}

//获取节点数据
std::string ZkClient::GetData(const std::string& path) {
    if (!is_connected_) {
        return "";
    }

    char buffer[MAXSIZE];
    int buffer_len = sizeof(buffer);
    int ret = zoo_get(zk_handle_, path.c_str(), 0, buffer, &buffer_len, nullptr);

    if (ret == ZOK) {
        return std::string(buffer, buffer_len);
    }
    //获取节点数据失败
    return "";
}

} // namespace mooncrpc