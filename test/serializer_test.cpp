#include "../src/log/log_manager.h"
#include "../src/serializer/serializer_manager.h"
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

using namespace mooncrpc;
using json = nlohmann::json;

int main() {
    //1.初始化日志模块
    if (!Logger::GetInstance().Init("./test_logs")) {
        return -1;
    }

    LOG_INFO("=== Starting Serializer Module Test ===");

    //阶段1：JSON 序列化与反序列化测试
    LOG_INFO("--- Phase 1: JSON Serialization Test ---");

    // 构造原始 JSON 数据
    json original_json;
    original_json["service"] = "AuthService";
    original_json["action"] = "Login";
    original_json["timestamp"] = 1718611200;

    // JSON 序列化
    std::string json_buffer = SerializerManager::Serialize(original_json, SerializeType::JSON);
    if (!json_buffer.empty()) {
        LOG_INFO("JSON Serialization SUCCESS. Buffer size: {} bytes", json_buffer.size());
    }

    // JSON 反序列化
    json parsed_json;
    if (SerializerManager::Deserialize(json_buffer, parsed_json, SerializeType::JSON)) {
        LOG_INFO("JSON Deserialization SUCCESS. Parsed service name: {}", parsed_json["service"].get<std::string>());
    } else {
        LOG_ERROR("JSON Deserialization FAILED!");
    }

    // 阶段2： Protobuf 序列化与反序列化测试
    LOG_INFO("--- Phase 2: Protobuf Serialization Test ---");

    //构造原始 Protobuf 数据
    mooncrpc::LoginRequest original_pb;
    original_pb.set_username("Moonc_Super_User");
    original_pb.set_password("Admin@123456");

    // Protobuf 序列化
    std::string pb_buffer = SerializerManager::Serialize(original_pb, SerializeType::PROTOBUF);
    if (!pb_buffer.empty()) {
        LOG_INFO("Protobuf Serialization SUCCESS. Buffer size: {} bytes", pb_buffer.size());
    }

    // Protobuf 反序列化
    mooncrpc::LoginRequest parsed_pb;
    if (SerializerManager::Deserialize(pb_buffer, parsed_pb, SerializeType::PROTOBUF)) {
        LOG_INFO("Protobuf Deserialization SUCCESS. Parsed username: {}", parsed_pb.username());
    } else {
        LOG_ERROR("Protobuf Deserialization FAILED!");
    }

    LOG_INFO("=== Serializer Module Test Completed Successfully ===");
    return 0;

}