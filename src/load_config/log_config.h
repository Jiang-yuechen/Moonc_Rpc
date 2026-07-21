#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace mooncrpc {

// 日志模块专属配置加载器
class LogConfig {
public:
    LogConfig() = default;
    ~LogConfig() = default;

    // 解析 JSON 配置文件
    bool InitLogConfig(const std::string& config_file);

    // Getter 方法
    std::string GetLogDir() const { return log_dir_; }
    std::string GetLogLevel() const { return log_level_; }
    bool IsAsyncMode() const { return async_mode_; }
    uint32_t GetAsyncQueueSize() const { return async_queue_size_; }
    uint32_t GetAsyncThreadCount() const { return async_thread_count_; }

    // 禁用拷贝与赋值
    LogConfig(const LogConfig&) = delete;
    LogConfig& operator=(const LogConfig&) = delete;

private:
    std::string log_dir_ = "./logs";        // 默认日志输出目录
    std::string log_level_ = "info";         // 默认日志级别
    bool async_mode_ = false;               // 默认关闭异步模式
    uint32_t async_queue_size_ = 8192;      // 异步队列大小(2 的幂)
    uint32_t async_thread_count_ = 1;       // 后台刷盘线程数
};

} // namespace mooncrpc