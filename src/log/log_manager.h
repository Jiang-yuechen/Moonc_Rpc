#pragma once

#include <iostream>
#include <filesystem>   //C++17文件操作系统
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>    //控制台信息类型彩色输出
#include <spdlog/sinks/daily_file_sink.h>       //按日期轮转的文件输出
#include <spdlog/async.h>                       // 引入 spdlog 的异步机制核心头文件

/**
 * @brief 基础日志宏定义
 * 提供不同级别的日志记录功能：
 * TRACE：最详细的日志级别，用于追踪程序执行流程
 * DEBUG：调试信息，用于开发调试
 * INFO：一般信息，记录程序正在运行状态
 * WARN：警告信息，表示潜在问题
 * ERROR：错误信息，表示程序运行出错
 * FATAL： 致命错误，程序无法继续运行
 */
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_FATAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

/** 
 * @brief 客户端专属日志宏定义
 * 在基础日志级别上添加 [CLIENT] 前缀，用于区分客户端日志
 * 利用 C++ 预处理器字符串字面量拼接特性，实现无缝格式化
*/
#define CLIENT_TRACE(...) SPDLOG_TRACE("[CLIENT] " __VA_ARGS__)
#define CLIENT_DEBUG(...) SPDLOG_DEBUG("[CLIENT] " __VA_ARGS__)
#define CLIENT_INFO(...)  SPDLOG_INFO("[CLIENT] " __VA_ARGS__)
#define CLIENT_WARN(...)  SPDLOG_WARN("[CLIENT] " __VA_ARGS__)
#define CLIENT_ERROR(...) SPDLOG_ERROR("[CLIENT] " __VA_ARGS__)
#define CLIENT_FATAL(...) SPDLOG_CRITICAL("[CLIENT] " __VA_ARGS__)

namespace mooncrpc {

class Logger {
public:
    // C++11线程安全的单例模式
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    // 初始化日志系统(console && file 同时记录)，默认读取../config/log_config.json
    bool Init(const std::string& config_file = "../config/log_config.json");

    // 禁用拷贝构造和拷贝赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;    // 私有构造函数
    ~Logger() = default;    // 私有析构函数

    std::shared_ptr<spdlog::logger> logger_;    // spdlog记录器实例
};

}   // namespace mooncrpc