#include "rpc_client.h"
#include "protocol/rpc_error_code.h"
#include "protocol/rpc_protocol.h"
#include "serializer/serializer_manager.h"
#include "conn_balancer/zk_conn_handler.h"
#include "network/create_socket.h"
#include "log/log_manager.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sys/socket.h>

namespace mooncrpc {

RpcClient::RpcClient(const std::string& config_file) {
    // 1. 读取并解析客户端配置文件
    // 使用 std:ifstream 结合 RAII 机制，确保文件句柄在离开作用域时自动释放
    std::ifstream file(config_file);
    if (!file.is_open()) {
        LOG_FATAL("Failed to open client config file: {}", config_file);
        exit(EXIT_FAILURE);
    }

    // 利用 nlohmann/json 解析 JSON
    // 使用 .value() 提供默认值，防止未配置字段导致崩溃
    nlohmann::json config = nlohmann::json::parse(file);
    std::string zk_host = config.value("zk_host", "127.0.0.1");
    uint16_t zk_port = config.value("zk_port", 2181);
    timeout_ms_ = config.value("timeout_ms", 3000);
    retry_times_ = config.value("retry_times", 3);

    std::string zk_addr = zk_host + ":" + std::to_string(zk_port);

    // 2. 初始化服务发现组件 (Zookeeper)
    // 客户端需要作为一个 ZK 的 Watcher/Client 连入集群，以便后续去拉取服务提供者的 IP 列表
    if (!ZkConnHandler::GetInstance().Init(zk_addr)) {
        LOG_FATAL("RpcClient failed to connect to Zookeeper: {}", zk_addr);
        exit(EXIT_FAILURE);
    }
    LOG_INFO("RpcClient initialized successfully. ZK: {}", zk_addr);
}

RpcClient::~RpcClient() {
    LOG_INFO("RpcClient shutting down...");
}

// 核心引擎：一次完整 RPC 调用的 6 大标准步骤
bool RpcClient::Call(const std::string& service_name,
                    const google::protobuf::Message& request,
                    google::protobuf::Message& response)
{
    // 1. 服务发现与负载均衡
    // 向 Zookeeper 询问：我想调用 service_name, 请给我一个目前可用且负载最低的服务端 IP:Port
    std::string target_server = ZkConnHandler::GetInstance().GetAvailableServer(service_name);
    if (target_server.empty()) {
        LOG_ERROR("Call failed: Service [{}] not found in registry.", service_name);
        return false;
    }

    // 字符串拆解：将 "IP:PORT" 解析为底层 Socket API 需要的独立变量
    size_t colon_pos = target_server.find(":");     // 寻找切分位置
    std::string ip = target_server.substr(0, colon_pos);
    uint16_t port = std::stoi(target_server.substr(colon_pos + 1));

    // 2. 业务数据序列化
    // 将强类型的 Protobuf 内存对象压缩转换未适合网络传输的扁平二进制字节流
    std::string serialized_req = SerializerManager::Serialize(request, SerializeType::PROTOBUF);
    if (serialized_req.empty()) {
        LOG_ERROR("Call failed: Serialize request error.");
        return false;
    }

    // 3. 协议封包(装信封)
    // 裸业务流在 TCP 中无法分割，必须套上自定义的 RpcHeader 物理包头
    RpcMessage req_msg;
    // 当前是简易同步模型，固定值不影响；未来扩展为异步回调模型时需要用雪花算法生成唯一 ID
    req_msg.header.seq_id = 1;
    req_msg.body = serialized_req;

    std::string send_buffer;
    if (!RpcProtocol::Encode(req_msg, send_buffer)) {
        LOG_ERROR("Call failed: Protocol Encode error.");
        return false;
    }

    // 4. 网络传输与重试机制
    std::string recv_buffer;
    bool network_success = false;

    // 重试循环
    for(uint32_t i = 0; i < retry_times_; i++) {
        // 短连接模式：每次 RPC 调用都会经历 建立连接->发送->接收->断开 的过程
        if (SendAndReceive(ip, port, send_buffer, recv_buffer)) {
            network_success = true;
            break;      // 成功立刻跳出重试循环
        }
        LOG_WARN("Network request failed, retrying...({}/{})", i + 1, retry_times_);
    }

    // 网络连接失败
    if (!network_success) {
        LOG_ERROR("Call failed: Network communication aborted after retries.");
        return false;
    }

    // 5. 协议拆包(拆信封)
    // 校验回包的魔数、长度是否合法，并剥离出 RpcHeader
    RpcMessage res_msg;
    if (!RpcProtocol::Decode(recv_buffer, res_msg)) {
        LOG_ERROR("Call failed: Protocol Decode error;");
    }

    // 检查服务端的框架层错误码 (eg. 服务不存在、鉴权失败、限流等)
    if (res_msg.header.error_code != static_cast<uint32_t>(RpcErrorCode::SUCCESS)) {
        LOG_ERROR("Server returned framework/business error code: {}", res_msg.header.error_code);
        return false;
    }

    // 6. 业务数据反序列化
    // 将二进制 payload 重新还原为调用者期望接收的 Protobuf 对象
    if (!SerializerManager::Deserialize(res_msg.body, response, SerializeType::PROTOBUF)){
        LOG_ERROR("Call failed: Deserialize response error.");
        return false;
    }

    LOG_INFO("RPC Call to [{}] completed successfully.", service_name);
    return true;
}

// 客户端底层的网络发报机
bool RpcClient::SendAndReceive(const std::string& ip, uint16_t port,
                                const std::string& send_buf, std::string& recv_buf)
{
    // 1. 这里不使用 epoll，而去创建阻塞 socket
    // 因为这通常是业务方的发起端线程 (例如游戏客户端、web 接口请求处理线程)
    // 在同步 RPC 语义下，调用方本来就需要阻塞等待结果，此时使用传统的阻塞式 Socket 是最轻量、最安全的做法
    // 无需引入 Reactor 状态机
    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        return false;
    }

    // 设置 SO_SNDTIMEO 和 SO_RCVTIMEO
    // 否则在极端的弱网环境，底层 connect 和 recv 可能挂起很长时间导致调用方线程池耗尽

    // 将毫秒 拆解为秒和微秒
    struct timeval tv;
    tv.tv_sec = timeout_ms_ / 1000;
    tv.tv_usec = (timeout_ms_ % 1000) * 1000;

    // 设置发送超时 (SO_SNDTIMEO)
    // Linux 中，SO_SNDTIMEO 顺便也能控制 connect() 的超时时间，避免建连阶段卡死
    if (::setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        LOG_ERROR("Failed to set SO_SNDTIMEO for FD: {}. Error: {}", client_fd, strerror(errno));
        SocketUtils::Close(client_fd);
        return false;
    }

    // 设置接收超时 (SO_RCVTIMEO)
    // 保证 recv() 最多阻塞 timeout_ms_ 毫秒，超时返回 -1并设置 errno 为 EAGAIN or EWOULDBLOCK
    if (::setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        LOG_ERROR("Failed to set SO_RCVTIMEO for FD: {}. Error: {}", client_fd, strerror(errno));
        return false;
    }

    // 2. 发起 TCP 三次握手
    if (!SocketUtils::Connect(client_fd, ip, port)) {
        SocketUtils::Close(client_fd);
        return false;
    }

    // 3. 将请求数据塞入内核发送缓冲区
    ssize_t send_len = ::send(client_fd, send_buf.data(), send_buf.size(), 0);
    if (send_len != static_cast<ssize_t>(send_buf.size())) {
        SocketUtils::Close(client_fd);
        return false;
    }

    // 4. 接收数据 (受 SO_RCVTIMEO 保护)
    char buf[4096];
    recv_buf.clear();
    while (true) {
        ssize_t recv_len = ::recv(client_fd, buf, sizeof(buf), 0);
        if (recv_len > 0) {
            recv_buf.append(buf, recv_len);

            // 包头检测，解决半包问题
            if (recv_buf.size() >= sizeof(RpcHeader)) {
                RpcHeader* header = reinterpret_cast<RpcHeader*>(recv_buf.data());
                if (recv_buf.size() >= sizeof(RpcHeader) + header->body_size) {
                    break;  // 数据接收完毕
                }
            }
        } else if (recv_len == 0) {
            // 对端正常断开
            break;
        } else {
            // recv_len < 0
            // 如果是因为设置了 SO_RCVTIMEO 导致超时，errno 会是 EAGAIN or EWOULDBLOCK
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_WARN("RPC recv timeout! Exceeded {} ms", timeout_ms_);
            } else {
                LOG_ERROR("RPC recv error for FD: {}. Error: {}", client_fd, strerror(errno));
            }
            break;
        }
    }

    SocketUtils::Close(client_fd);

    // 只有在接收到完整包头并且实际数据长度等于声明的包体长度时才算真正成功
    if (recv_buf.size() >= sizeof(RpcHeader)) {
        RpcHeader* header = reinterpret_cast<RpcHeader*>(recv_buf.data());
        if (recv_buf.size() == sizeof(RpcHeader) + header->body_size) {
            return true;
        }
    }

    return false;
}

} // namespace mooncrpc