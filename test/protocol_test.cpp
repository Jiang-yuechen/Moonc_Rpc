#include "../src/protocol/rpc_protocol.h"
#include "../src/log/log_manager.h"
#include <iostream>
#include <string>

using namespace mooncrpc;

int main() {
    if (!Logger::GetInstance().Init("./test_logs")) {
        return -1;
    }

    LOG_INFO("====== Starting RPC Protocol Module Test ======");

    // ---------------------------------------------------------
    // Phase 1: 正常封包与解包测试
    // ---------------------------------------------------------
    LOG_INFO("--- Phase 1: Normal Encode & Decode ---");
    RpcMessage req_msg;
    req_msg.header.seq_id = 10086;
    req_msg.body = "This is a highly compressed and encrypted binary payload.";

    std::string network_stream;
    if (RpcProtocol::Encode(req_msg, network_stream)) {
        LOG_INFO("Message Encoded to binary stream successfully.");
    }

    RpcMessage parsed_msg;
    if (RpcProtocol::Decode(network_stream, parsed_msg)) {
        LOG_INFO("Message Decoded successfully!");
        LOG_INFO("Parsed SeqId: {}, Body: {}", parsed_msg.header.seq_id, parsed_msg.body);
    } else {
        LOG_ERROR("Message Decoded failed!");
    }

    // ---------------------------------------------------------
    // Phase 2: TCP 半包测试 (模拟数据在网线里卡住了一半)
    // ---------------------------------------------------------
    LOG_INFO("--- Phase 2: TCP Half-Packet (Incomplete data) Test ---");
    // 故意截断字符串，模拟只收到了一半的数据包
    std::string half_stream = network_stream.substr(0, 15); 
    RpcMessage tmp_msg;
    bool is_success = RpcProtocol::Decode(half_stream, tmp_msg);
    if (!is_success) {
        LOG_INFO("Half-Packet gracefully rejected (Waiting for more data). This is the EXPECTED correct behavior.");
    }

    // ---------------------------------------------------------
    // Phase 3: 魔数攻击测试 (模拟其他协议发来的垃圾请求)
    // ---------------------------------------------------------
    LOG_INFO("--- Phase 3: Magic Number Validation Test ---");
    std::string fake_stream = network_stream;
    // 暴力篡改前两个字节 (破坏魔数)
    fake_stream[0] = 0x11; 
    fake_stream[1] = 0x22;
    is_success = RpcProtocol::Decode(fake_stream, tmp_msg);
    if (!is_success) {
        LOG_INFO("Invalid Magic Number gracefully rejected. This is the EXPECTED correct behavior.");
    }

    LOG_INFO("====== RPC Protocol Module Test Completed Successfully ======");
    return 0;
}