#include "log_config.h"
#include <fstream>
#include <iostream>

namespace mooncrpc {

bool LogConfig::InitLogConfig(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open log config file: " << config_file << ". Using default settings." << std::endl;
        return false;
    }

    try {
        nlohmann::json config = nlohmann::json::parse(file);

        log_dir_ = config.value("log_dir", "./logs");
        log_level_ = config.value("log_value", "info");
        async_mode_ = config.value("async_mode", false);
        async_queue_size_ = config.value("async_queue_size", 8192);     // 必须是 2 的幂次
        async_thread_count_ = config.value("async_thread_count", 1);

        return true;
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error in log_config: " << e.what() << std::endl;
        return false;
    }
}

} // namespace mooncrpc