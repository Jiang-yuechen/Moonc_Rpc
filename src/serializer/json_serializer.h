#pragma once
#include "../log/log_manager.h"
#include <nlohmann/json.hpp>
#include <string>

namespace mooncrpc {

/**
 * @brief JSON 序列化器
 * @details 负责处理文本协议的数据转换。由于 nlohmann::json 提供了极佳的异常处理机制，
 * 我们在这里捕获异常，防止非法的 JSON 格式导致整个 RPC 进程崩溃。
 */
class JSONSerializer {
public:
    //将 nlohmann::json 对象序列化为字符串方法
    template <typename T>
    static std::string Serialize(const T& data) {
        try {
            nlohmann::json j = data;
            return j.dump();    // dump() 方法将内存结构转化成紧凑的字符串
        } catch (const std::exception& e) {
            LOG_ERROR("JSON serialization failed: {}", e.what());
            return "";
        }
    }

    //将字符串反序列化为 nlohmann::json 对象方法
    template <typename T>
    static bool Deserialize(const std::string& buffer, T& data) {
        try {
            // parse(解析):从文本到对象
            nlohmann::json j = nlohmann::json::parse(buffer);
            //用 get<T>() 进行显式转换类型
            data = j.get<T>();
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("JSON deserialization failed: {}", e.what());
            return false;
        }
    }
};

} //namespace mooncrpc