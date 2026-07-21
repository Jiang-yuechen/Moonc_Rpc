#pragma once
#include "json_serializer.h"
#include "protobuf_serializer.h"
#include "log/log_manager.h"
#include <concepts>     //引入 C++20 概念库

namespace mooncrpc {

//序列化类型枚举，后续写入网络协议头 (RpcHeader) 中
enum class SerializeType : uint8_t {
    JSON = 0,
    PROTOBUF = 1
};

/**
 * @brief 序列化门面类
 * @details 提供统一的序列化入口
 * 使用 C++20 concepts 约束模板参数，避免传入错误类型导致模板编译错误
 */
class SerializerManager {
public:
    /**
     * @brief 统一序列化入口
     * @tparam T 待序列化的数据类型
     * @param data 待序列化的数据对象
     * @param type 期望使用的序列化方式
     */
    template <typename T>
    static std::string Serialize(const T& data, SerializeType type) {
        try {
            switch (type) {
                case SerializeType::JSON:
                    //编译器校验：只有 T 是 nlohmann::json 类型时才允许编译这条分支
                    if constexpr (std::same_as<T, nlohmann::json>) {
                        return JSONSerializer::Serialize(data);
                    } else {
                        LOG_ERROR("Type mismatch: Expected nlohmann::json for JSON serialization");
                        return "";
                    }
                
                case mooncrpc::SerializeType::PROTOBUF:
                    //编译器校验：只有 T 继承自 google::protobuf::Message 时才允许编译
                    if constexpr (std::derived_from<T, google::protobuf::Message>) {
                        return ProtobufSerializer::Serialize(data);
                    } else {
                        LOG_ERROR("Type mismatch: Expected Protobuf Message for PROTOBUF serialization");
                        return "";
                    }
                
                default:
                    LOG_ERROR("Unknown serialization type");
                    return "";
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Manager serialization exception: {}", e.what());
            return "";
        }
    }

    /**
     * @brief 统一反序列化入口
     * @tparam T 目标数据类型
     * @param buffer 序列化后的字符串
     * @param data 输出参数：反序列化后的数据
     * @param type 序列化类型
     */
    template <typename T>
    static bool Deserialize(const std::string& buffer, T& data, SerializeType type) {
        try {
            switch (type) {
                case SerializeType::JSON:
                    //编译时类型检查
                    if constexpr (std::same_as<T, nlohmann::json>) {
                        return JSONSerializer::Deserialize(buffer, data);
                    } else {
                        LOG_ERROR("Type mismatch: Expected nlohmann::json for JSON deserialization");
                        return false;
                    }

                case SerializeType::PROTOBUF:
                    //编译时类型检查
                    if constexpr (std::derived_from<T, google::protobuf::Message>) {
                        return ProtobufSerializer::Deserialize(buffer, data);
                    } else {
                        LOG_ERROR("Type mismatch: Expected Protobuf Message for PROTOBUF deserialization");
                        return false;
                    }

                default:
                    LOG_ERROR("Unknown serialization type");
                    return false;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Manager deserialization exception: {}", e.what());
            return "";
        }
    }
};

} //namespace mooncrpc