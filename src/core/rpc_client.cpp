#include "rpc_client.h"
#include "protocol/rpc_error_code.h"
#include "protocol/rpc_protocol.h"
#include "serializer/serializer_manager.h"
#include "conn_balancer/zk_conn_handler.h"
#include "network/create_socket.h"
#include "log/log_manager.h"
#include "compress_data/zstd_compress.h"
#include "encrypt/aes_encrypt.h"

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

    // 2. 初始化 LoadBalancer 引擎
    std::string lb_type = config.value("load_balancer", nlohmann::json::object()).value("type", "random");
    LoadBalancer::initBalancer(lb_type);

    // 如果是 weight 类型，将权重表注入进去
    if (lb_type == "weight") {
        auto instance = LoadBalancer::getInstance();
        if (instance) {
            std::unordered_map<std::string, int> weights_map;
            // 将从 json 中读出来的权重赋值进去
            for (auto& el : config["load_balancer"]["weights"].items()) {
                weights_map[el.key()] = el.value().get<int>();
            }
            instance->initWeights(weights_map);
        }
    }

    // 3. 初始化服务发现组件 (Zookeeper)
    // 客户端需要作为一个 ZK 的 Watcher/Client 连入集群，以便后续去拉取服务提供者的 IP 列表
    if (!ZkConnHandler::GetInstance().Init(zk_addr)) {
        LOG_FATAL("RpcClient failed to connect to Zookeeper: {}", zk_addr);
        exit(EXIT_FAILURE);
    }
    LOG_INFO("RpcClient initialized successfully. ZK: {}", zk_addr);
}

RpcClient::~RpcClient() {
    // 关闭所有长连接
    for (auto& pair : connection_pool_) {
        SocketUtils::Close(pair.second);
    }
    connection_pool_.clear();
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

    // 2. 业务数据序列化 -> 动态压缩(附带兜底策略) -> 加密
    // 将强类型的 Protobuf 内存对象压缩转换未适合网络传输的扁平二进制字节流
    std::string serialized_req = SerializerManager::Serialize(request, SerializeType::PROTOBUF);
    if (serialized_req.empty()) {
        LOG_ERROR("Call failed: Serialize request error.");
        return false;
    }

    // 动态压缩
    std::string processed_req;
    CompressType current_compress_type = CompressType::NONE;
    constexpr size_t COMPRESS_THRESHOLD = 4096;      // 压缩阈值为 4KB

    if (serialized_req.size() > COMPRESS_THRESHOLD) {
        if (ZstdCompress::GetInstance().CompressString(serialized_req, processed_req)) {
            current_compress_type = CompressType::ZSTD;     // 使用 Zstd 压缩
            CLIENT_DEBUG("Payload compressed: {}B -> {}B", serialized_req.size(), processed_req.size());
        } else {
            // 压缩失败，降级为明文发送，不丢弃数据
            CLIENT_WARN("Compression failed! Falling back to uncompressed mode.");
            processed_req = serialized_req;
            current_compress_type = CompressType::NONE;
        }
    } else {
        // 未达到压缩条件，直接透传，节省 CPU
        processed_req = serialized_req;
        current_compress_type = CompressType::NONE;
    }

    // 加密
    std::string encrypted_req;
    if (!AesEncrypt::GetInstance().Encrypt(processed_req, encrypted_req)) {
        CLIENT_ERROR("Encryption failed!");
        return false;
    }

    // 3. 协议封包(装信封)
    // 裸业务流在 TCP 中无法分割，必须套上自定义的 RpcHeader 物理包头
    RpcMessage req_msg;
    // 当前是简易同步模型，固定值不影响；未来扩展为异步回调模型时需要用雪花算法生成唯一 ID
    req_msg.header.seq_id = 1;
    req_msg.header.compress_type = static_cast<uint8_t>(current_compress_type);
    req_msg.body = encrypted_req;

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
        CLIENT_WARN("Network request failed, retrying...({}/{})", i + 1, retry_times_);
    }

    // 网络连接失败
    if (!network_success) {
        CLIENT_ERROR("Call failed: Network communication aborted after retries.");
        return false;
    }

    // 5. 协议拆包(拆信封)
    // 校验回包的魔数、长度是否合法，并剥离出 RpcHeader
    RpcMessage res_msg;
    if (!RpcProtocol::Decode(recv_buffer, res_msg)) {
        CLIENT_ERROR("Call failed: Protocol Decode error.");
        return false;
    }

    // 检查服务端的框架层错误码 (eg. 服务不存在、鉴权失败、限流等)
    if (res_msg.header.error_code != static_cast<uint32_t>(RpcErrorCode::SUCCESS)) {
        CLIENT_ERROR("Server returned framework/business error code: {}", res_msg.header.error_code);
        return false;
    }

    // 6. 业务数据: 解密 -> 动态解压 -> 反序列化
    // 解密
    std::string decrypted_res;
    if (!AesEncrypt::GetInstance().Decrypt(res_msg.body, decrypted_res)) {
        CLIENT_ERROR("Decryption failed!");
        return false;
    }

    // 动态解压
    std::string decompressed_res;
    if (res_msg.header.compress_type == static_cast<uint8_t>(CompressType::ZSTD)) {
        if (!ZstdCompress::GetInstance().DecompressString(decrypted_res, decompressed_res)) {
            // 数据受损或被恶意篡改，终止解析
            CLIENT_ERROR("Decompression failed! Data might be corrupted.");
            return false;
        }
    } else {
        // 对方标记没有压缩，直接透传
        decompressed_res = decrypted_res;
    }

    // 反序列化：将二进制 payload 重新还原为调用者期望接收的 Protobuf 对象
    if (!SerializerManager::Deserialize(decompressed_res, response, SerializeType::PROTOBUF)){
        CLIENT_ERROR("Call failed: Deserialize response error.");
        return false;
    }

    CLIENT_INFO("RPC Call to [{}] completed successfully.", service_name);
    return true;
}

// 客户端底层的网络发报机
bool RpcClient::SendAndReceive(const std::string& ip, uint16_t port,
                                const std::string& send_buf, std::string& recv_buf)
{
    std::string server_key = ip + ":" + std::to_string(port);
    int client_fd = -1;

    // 1. 尝试从连接池中获取复用的长连接
    auto it = connection_pool_.find(server_key);
    if (it != connection_pool_.end()) {
        client_fd = it->second;
    } else {
        // 没有连接可以复用，创建新连接
        client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd < 0) {
            return false;
        }

        // 将毫秒 拆解为秒和微秒
        struct timeval tv;
        tv.tv_sec = timeout_ms_ / 1000;
        tv.tv_usec = (timeout_ms_ % 1000) * 1000;
        
        // 设置 SO_SNDTIMEO 和 SO_RCVTIMEO
        // 否则在极端的弱网环境，底层 connect 和 recv 可能挂起很长时间导致调用方线程池耗尽
    
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
        // 建立成功，塞入连接池
        connection_pool_[server_key] = client_fd;
    }

    // 3. 将请求数据塞入内核发送缓冲区
    ssize_t send_len = ::send(client_fd, send_buf.data(), send_buf.size(), 0);
    if (send_len != static_cast<ssize_t>(send_buf.size())) {
        // 如果发送失败(可能对端挂断导致连接失败), 立即销毁死连接
        SocketUtils::Close(client_fd);
        connection_pool_.erase(server_key);
        return false;       // 返回false，外层重试机制会触发重新建连
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
            // 对端正常断开连接，清理连接池
            SocketUtils::Close(client_fd);
            connection_pool_.erase(server_key);
            return false;
        } else {
            // recv_len < 0
            // 如果是因为设置了 SO_RCVTIMEO 导致超时，errno 会是 EAGAIN or EWOULDBLOCK
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_WARN("RPC recv timeout! Exceeded {} ms", timeout_ms_);
            } else {
                LOG_ERROR("RPC recv error for FD: {}. Error: {}", client_fd, strerror(errno));
            }
            // 清理连接池
            SocketUtils::Close(client_fd);
            connection_pool_.erase(server_key);
            return false;
        }
    }

    // SocketUtils::Close(client_fd); // 这里不Close，fd留给下一次请求复用

    // 只有在接收到完整包头并且实际数据长度等于声明的包体长度时才算真正成功
    if (recv_buf.size() >= sizeof(RpcHeader)) {
        RpcHeader* header = reinterpret_cast<RpcHeader*>(recv_buf.data());
        if (recv_buf.size() == sizeof(RpcHeader) + header->body_size) {
            return true;
        }
    }

    // 使用脏数据兜底
    SocketUtils::Close(client_fd);
    connection_pool_.erase(server_key);
    return false;
}

} // namespace mooncrpc