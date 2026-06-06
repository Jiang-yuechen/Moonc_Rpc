#include "log_manager.h"
#include <exception>
#include <filesystem>
#include <memory>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/spdlog.h>

namespace mooncrpc {

bool Logger::Init(const std::string& log_dir) {
    try {
        //1.创建日志目录(C++17)
        std::filesystem::path log_path(log_dir);
        if (!std::filesystem::exists(log_path)) {   //不存在当前目录则新建一个
            std::filesystem::create_directories(log_path);
        }

        //2.创建控制台输出槽(sink)
        //对于sink版本：mt_类型内部使用互斥锁，线程安全，适用于多线程
        //st_类型没有锁开销，性能最高但只适用于单线程
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        //3.创建文件输出槽(每天零点创建一个新日志文件)
        std::string log_file = log_dir + "/mooncrpc.log";
        auto file_sink = std::make_shared<spdlog::sinks::daily_file_format_sink_mt>(log_file, 0, 0);

        //4.组合多个sink创建Logger
        std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};
        logger_ = std::make_shared<spdlog::logger>("main_logger", sinks.begin(), sinks.end());

        //5.设置日志格式：[时间] [级别] [线程] 消息
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

        //6.设置最低打印级别:info
        logger_->set_level(spdlog::level::info);

        //7.设置为默认Logger，这样宏定义才会生效
        spdlog::set_default_logger(logger_);

        return true;
    } catch (const std::exception& e) {
        //初始化失败输出错误信息
        std::cerr << "Logger initialization failed:" << e.what() << std::endl;
        return false;
    }
}

}