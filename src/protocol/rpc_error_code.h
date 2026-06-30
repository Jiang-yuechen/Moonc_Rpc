#pragma once
#include <cstdint>
#include <string>

namespace mooncrpc {

// 全局错误码定义
enum class RpcErrorCode : uint32_t {
    SUCCESS = 0,

    // 客户端/网络层错误 (1000 级别)
    NETWORK_CONNECT_ERROR = 1001,       // 无法连接到服务器
    NETWORK_TIMEOUT = 1002,             // 网络请求超时
    NETWORK_SEND_ERROR = 1003,          // 网络发送数据失败
    NETWORK_RECV_ERROR = 1004,          // 网络接收数据失败

    // 路由与发现层错误 (2000 级别)
    SERVICE_NOT_FOUND = 2001,           // 服务未找到 (请求了未注册服务)
    METHOD_NOT_FOUND = 2002,            // 方法未找到
    DISCOVER_FAILED = 2003,             // 服务发现失败 (Zookeeper 中无可用节点)

    // 业务与编解码层错误 (3000 级别)
    DESERIALIZE_FAILED = 3001,          // 协议反序列化失败
    INTERNAL_ERROR = 3002,              // 服务端内部业务逻辑报错异常
    ENCRYPT_COMPRESS_ERROR = 3003       // 加密或压缩流水线处理失败
};

// 辅助函数：将错误码转换为可读的字符串，方便日志打印
inline std::string GetRpcErrorString(RpcErrorCode code) {
    switch (code) {
        case RpcErrorCode::SUCCESS: return "SUCCESS";
        case RpcErrorCode::NETWORK_CONNECT_ERROR: return "NETWORK_CONNECT_ERROR";
        case RpcErrorCode::NETWORK_TIMEOUT: return "NETWORK_TIMEOUT";
        case RpcErrorCode::NETWORK_SEND_ERROR: return "NETWORK_SEND_ERROR";
        case RpcErrorCode::NETWORK_RECV_ERROR: return "NETWORK_RECV_ERROR";
        case RpcErrorCode::SERVICE_NOT_FOUND: return "SERVICE_NOT_FOUND";
        case RpcErrorCode::METHOD_NOT_FOUND: return "METHOD_NOT_FOUND";
        case RpcErrorCode::DISCOVER_FAILED: return "DISCOVER_FAILED";
        case RpcErrorCode::DESERIALIZE_FAILED: return "DESERIALIZE_FAILED";
        case RpcErrorCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
        case RpcErrorCode::ENCRYPT_COMPRESS_ERROR: return "ENCRYPT_COMPRESS_ERROR";
        default: return "UNKNOWN_ERROR";
    }
}

} // namespace mooncrpc