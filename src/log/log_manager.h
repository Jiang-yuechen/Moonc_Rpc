#pragma once

#include <iostream>
#include <filesystem>   //C++17文件操作系统
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>    //控制台信息类型彩色输出
#include <spdlog/sinks/daily_file_sink.h>       //按日期轮转的文件输出

//基础日志宏，使用更方便，可以自动附带文件名、行号等上下文信息
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_FATAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

namespace mooncrpc {

class Logger {
public:
    //C++11线程安全的单例模式
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }

    //初始化日志系统(console && file 同时记录)
    bool Init(const std::string& log_dir = "./logs");

    //禁用拷贝构造和拷贝赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;    //私有构造函数
    ~Logger() = default;    //私有析构函数

    std::shared_ptr<spdlog::logger> logger_;    //spdlog记录器实例
};

}   //namespace mooncrpc