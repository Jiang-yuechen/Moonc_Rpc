#include "log_manager.h"
#include "load_config/log_config.h"
#include <exception>
#include <memory>

namespace mooncrpc {

bool Logger::Init(const std::string& config_file) {
    try {
        // 1. 加载动态日志配置
        LogConfig config;
        config.InitLogConfig(config_file);

        // 创建并校验日志目录
        std::filesystem::path log_path(config.GetLogDir());
        if (!std::filesystem::exists(log_path)) {   //不存在当前目录则新建一个
            std::filesystem::create_directories(log_path);
        }

        //2.创建控制台输出槽(sink)
        //对于sink版本：mt_类型内部使用互斥锁，线程安全，适用于多线程
        //st_类型没有锁开销，性能最高但只适用于单线程
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        //3.创建文件输出槽(每天零点创建一个新日志文件)
        std::string log_file = config.GetLogDir() + "/mooncrpc.log";
        // 配置文件滚动策略
        auto file_sink = std::make_shared<spdlog::sinks::daily_file_format_sink_mt>(log_file, 0, 0);
        std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};

        // 4. 根据配置决定是否开启异步日志
        if (config.IsAsyncMode()) {
            // 初始化全局线程池，分配队列大小和后台刷盘线程数
            spdlog::init_thread_pool(config.GetAsyncQueueSize(), config.GetAsyncThreadCount());

            // 使用 async_logger 替换普通的 logger
            // block 策略：当异步队列满时，阻塞调用者，保证日志不丢失
            logger_ = std::make_shared<spdlog::async_logger>(
                "mooncrpc_async_logger",
                sinks.begin(), sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block
            );
            std::cout << "[Logger] Async mode enabled. Queue size: " << config.GetAsyncQueueSize() << std::endl;
        } else {
            // 开启同步模式
            logger_ = std::make_shared<spdlog::logger>("mooncrpc_sync_logger", sinks.begin(), sinks.end());
            std::cout << "[Logger] Sync mode enabled." << std::endl;
        }


        // 5. 设置日志格式：[时间] [级别] [Thread-线程ID] 消息
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [Thread-%t] %v");

        // 6. 动态设置日志打印级别
        if (config.GetLogLevel() == "debug") {
            logger_->set_level(spdlog::level::debug);
        } else {
            logger_->set_level(spdlog::level::info);
        }
        
        // 7. 设置为默认Logger，这样宏定义才会生效，并强制遇到 error 级别立即刷盘
        spdlog::set_default_logger(logger_);
        spdlog::flush_on(spdlog::level::err);

        return true;
    } catch (const std::exception& e) {
        //初始化失败输出错误信息
        std::cerr << "Logger initialization failed:" << e.what() << std::endl;
        return false;
    }
}

}