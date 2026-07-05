//处理工业级微服务最核心的 Protobuf 二进制序列化
#pragma once
#include "../log/log_manager.h"
#include "../protos/message.pb.h"
#include <google/protobuf/message.h>    //引入 Protobuf 的基类
#include <string>

namespace mooncrpc {

/**
 * @brief Protobuf 序列化器
 * @details 专门处理继承自 google::protobuf::Message 的生成类
 * Protobuf 序列化后的体积远小于 JSON，是高并发场景的主力
 */
class ProtobufSerializer {
public:
    /**
     * @brief 序列化 Protobuf 消息为二进制字符串
     * @tparam T Protobuf 消息类型(必须继承自 google::protobuf::Message)
     * @param data 需要被序列化的消息
     * @return std::string 序列化后的二进制字符串
     */
    template <typename T>
    static std::string Serialize(const T& data) {
        try {
            return data.SerializeAsString();
        } catch (const std::exception& e) {
            LOG_ERROR("Protobuf serialization failed: {}", e.what());
            return "";
        }
    }

    /**
     * @brief 将二进制字符串反序列化为 Protobuf 对象
     * @tparam T Protobuf 消息类型
     * @param buffer 二进制字符串
     * @param data 输出参数，反序列化后的消息
     */
    template <typename T>
    static bool Deserialize(const std::string& buffer, T& data) {
        try {
            // T new_data;
            // // ParseFromString 会自动校验字节流是否符合 proto 协议定义
            // if (!new_data.ParseFromString(buffer)) {
            //     LOG_ERROR("Failed to parse protobuf message from buffer.");
            //     return false;
            // }
            // data = std::move(new_data);
            // return true;

            // ParseFromString 是 MessageLite 定义的虚函数。
            // 即便传入的是基类 Message 的引用，由于多态的存在，
            // 它也会准确地调用具体子类（如 LoginResponse）的反序列化逻辑！
            // 并且 ParseFromString 内部会自动调用 Clear() 清理老数据，非常安全。
            return data.ParseFromString(buffer);
        } catch (const std::exception& e) {
            LOG_ERROR("Protobuf deserialization failed: {}", e.what());
            return false;
        }
    }
};

} //namespace mooncrpc
