#include "service_manager.h"
#include "compress_data/zstd_compress.h"
#include "encrypt/aes_encrypt.h"
#include "protocol/rpc_error_code.h"
#include "log/log_manager.h"

namespace mooncrpc {

// 初始化服务管理器，注入底层的异步线程池引擎
void ServiceManager::Init(std::shared_ptr<ThreadPool> thread_pool) {
    //使用 std::move 转移智能指针所有权，避免不必要的拷贝和引用计数增加开销
    thread_pool_ = std::move(thread_pool);
    LOG_INFO("ServiceManager initialized with async ThreadPool engine.");
}

// 注册本地 RPC 服务实例，供外部网络调用
bool ServiceManager::RegisterService(std::shared_ptr<Service> service) {
    //基础校验：防止空指针导致后续路由时的解引用崩溃
    if (!service) {
        LOG_ERROR("Cannot register a null service pointer.");
        return false;
    }

    //获取服务名作为哈希表路由的唯一表示 (key)
    std::string service_name = service->GetServiceName();

    {
        //使用 RAII 机制加锁，保护 service_map_
        //防止多个线程同时注册服务时触发底层容器扩容，导致数据竞争或迭代器失效
        std::lock_guard<std::mutex> lock(mutex_);
    
        //检查服务是否注册过，拒绝重复注册，确保服务路由的唯一性
        if (service_map_.find(service_name) != service_map_.end()) {
            LOG_WARN("Service [{}] is already registered.", service_name);
            return false;
        }
    
        //将服务实例存入本地注册表
        service_map_[service_name] = service;
        LOG_INFO("Successfully registered local RPC service: [{}]", service_name);
        return true;
    }
}

// 根据服务名称，从本地注册表中提取对应的服务实例
std::shared_ptr<Service> ServiceManager::GetService(const std::string& service_name) {
    //给读取操作加锁，防止读取瞬间其他线程写入数据
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = service_map_.find(service_name);
    if (it != service_map_.end()) {
        return it->second;
    }

    LOG_ERROR("Service not find in local registry: [{}]", service_name);
    return nullptr;
}

//同步处理 RPC 请求 (执行过程会阻塞当前调用线程)
void ServiceManager::HandleRpcRequestSync(const std::string& service_name,
                                        const RpcMessage& request, RpcMessage& response) {
    auto service = GetService(service_name);
    if (!service) {
        //服务未找到，构造错误相应包
        //假设 2001 表示未找到该服务，错误码体系后面在 core 中构建
        response.header.error_code = static_cast<uint32_t>(RpcErrorCode::SERVICE_NOT_FOUND);
        response.body = "ERROR: Service not found.";

        // 发送错误消息前也加密，对齐客户端统一的解密流水线
        std::string encrypted_err;
        if (AesEncrypt::GetInstance().Encrypt(response.body, encrypted_err)) {
            response.body = encrypted_err;
        }
        return;
    }

    // 1. 接收流水线：解密 -> 动态解压
    std::string decrypted_req;
    if (!AesEncrypt::GetInstance().Decrypt(request.body, decrypted_req)) {
        LOG_ERROR("Failed to decrypt request for service [{}]", service_name);
        response.header.error_code = static_cast<uint32_t>(RpcErrorCode::ENCRYPT_COMPRESS_ERROR);
        response.body = "ERROR: Decryption failed.";
        
        // 对错误消息进行加密
        std::string encrypted_err;
        if (AesEncrypt::GetInstance().Encrypt(response.body, encrypted_err)) {
            response.body = encrypted_err;
        }
        return;
    }

    std::string decompressed_req;
    if (request.header.compress_type == static_cast<uint8_t>(CompressType::ZSTD)) {
        if (!ZstdCompress::GetInstance().DecompressString(decrypted_req, decompressed_req)) {
            // 解压缩出错
            LOG_ERROR("Failed to decompress request for service [{}]", service_name);
            response.header.error_code = static_cast<uint32_t>(RpcErrorCode::ENCRYPT_COMPRESS_ERROR);
            response.body = "ERROR: Decompression failed.";
            std::string encrypted_err;
            if (AesEncrypt::GetInstance().Encrypt(response.body, encrypted_err)) {
                response.body = encrypted_err;
                return;
            }
        }
    } else {
        decompressed_req = decrypted_req; // 客户端未压缩，直接透传
    }

    // 构造明文 Request 给业务层
    RpcMessage plain_request = request;
    plain_request.body = decompressed_req;

    // 2. 执行真正的业务逻辑
    //防御性编程：捕获业务代码中可能抛出的所有标准异常
    //确保单一请求的业务 BUG 不会导致整个 RPC 服务端进程崩溃
    try {
        //多态调用：转入具体业务类重写的执行逻辑
        service->HandleRpcRequest(plain_request, response);
    } catch (const std::exception& e) {
        LOG_ERROR("Exception occurred while handling request in service [{}]: {}", service_name, e.what());
        response.header.error_code = static_cast<uint32_t>(RpcErrorCode::INTERNAL_ERROR);; // 服务端内部业务逻辑错误
        response.body = std::string("ERROR: ") + e.what();
    }

    // 3. 发送流水线：动态压缩(带兜底策略) -> 加密
    if (response.header.error_code == static_cast<uint32_t>(RpcErrorCode::SUCCESS)) {
        std::string processed_res;
        CompressType current_compress_type = CompressType::NONE;
        constexpr size_t COMPRESS_THRESHOLD = 4096; // 4096 字节阈值

        if (response.body.size() > COMPRESS_THRESHOLD) {
            // 超过透传阈值，需要压缩
            if (ZstdCompress::GetInstance().CompressString(response.body, processed_res)) {
                current_compress_type = CompressType::ZSTD;
                LOG_DEBUG("Response compressed: {}B -> {}B", response.body.size(), processed_res.size());
            } else {
                LOG_WARN("Response compression failed! Falling back to uncompressed mode.");
                processed_res = response.body;
                current_compress_type = CompressType::NONE;
            }
        } else {
            processed_res = response.body;
        }

        // 加密
        std::string encrypted_res;
        if (AesEncrypt::GetInstance().Encrypt(processed_res, encrypted_res)) {
            response.body = encrypted_res;
            response.header.compress_type = static_cast<uint8_t>(current_compress_type);
        } else {
            // 加密失败
            LOG_ERROR("Failed to encrypt response for service [{}]", service_name);
            response.header.error_code = static_cast<uint32_t>(RpcErrorCode::ENCRYPT_COMPRESS_ERROR);
            response.body = "ERROR: Encryption failed.";
            response.header.compress_type = static_cast<uint8_t>(CompressType::NONE);
            std::string encrypted_err;
            if (AesEncrypt::GetInstance().Encrypt(response.body, encrypted_err)) response.body = encrypted_err;
        }
    } else {
        // 若业务抛错，错误信息（简短字符串）无需压缩，直接加密返回以对齐客户端
        std::string encrypted_err;
        if (AesEncrypt::GetInstance().Encrypt(response.body, encrypted_err)) {
            response.body = encrypted_err;
            response.header.compress_type = static_cast<uint8_t>(CompressType::NONE);
        }
    }

}

//异步处理 RPC 请求(将任务装载到线程池，不阻塞底层的网络 I/O 线程)
void ServiceManager::HandleRpcRequestAsync(const std::string& service_name,
                                            const RpcMessage& request,
                                            std::function<void(const RpcMessage&)> callback) {
    if (!thread_pool_) {
        LOG_ERROR("ThreadPool is not initialized. Async request for [{}] failed.", service_name);
        //优雅降级：若未配置线程池，则退化为同步处理方式并立即回调，避免直接丢弃客户端请求
        RpcMessage response;
        response.header.seq_id = request.header.seq_id;
        response.header.msg_type = static_cast<uint8_t>(MsgType::RESPONSE);
        HandleRpcRequestSync(service_name, request, response);
        if (callback) {
            callback(response);
        }
        return;
    }

    //核心异步流转逻辑：将请求上下文包装被闭包任务，扔进 ThreadPool
    //按值捕获 request 和 service_name，确保任务在未来某个子线程执行时，这些局部变量不会因为当前函数的出栈而销毁 (悬垂引用)
    thread_pool_->Enqueue(TaskPriority::NORMAL, [this, service_name, request, callback]() {
        RpcMessage response;
        //继承请求的序列号，网络层通过 seq_id 将响应精准路由回对应的客户端请求
        response.header.seq_id = request.header.seq_id;
        //自动将数据包头类型标记为 RESPONSE
        response.header.msg_type = static_cast<uint8_t>(MsgType::RESPONSE);

        //在子线程 (消费者线程) 中实际执行同步的业务逻辑
        this->HandleRpcRequestSync(service_name, request, response);

        //业务逻辑执行完毕后，触发回调函数
        //通常这个 callback 内部包含网络层的 Send 逻辑，负责将 res 发送回客户端
        if (callback) {
            callback(response);
        }
    });
}

//获取底层线程池的实时运行监控数据
ThreadPoolStats ServiceManager::GetThreadPoolStats() const {
    //委托给内部的 thread_pool_ 实例去获取
    if (thread_pool_) {
        return thread_pool_->GetStats();
    }
    return ThreadPoolStats{};
}

} //namespace mooncrpc