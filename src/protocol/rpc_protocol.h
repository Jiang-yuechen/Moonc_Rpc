#pragma once
#include "../serializer/serializer_manager.h"
#include <string>
#include <cstdint>
#include <vector>

namespace mooncrpc {

//魔数定义，用于快速过滤非本框架的垃圾请求
//'M' = 0x4D, 'C' = 0x43 (代表 MooncRPC)
constexpr uint16_t RPC_MAGIC_NUMBER = 0x4D43;

//消息类型枚举
enum class MsgType : uint8_t {
    REQUEST = 1,
    RESPONSE = 2
};

//RPC 物理包头定义 (定长部分)
//强制 1 字节对齐，防止编译器自动插入 padding 导致跨语言解析失败
//使用 push/pop 组合限制自定义规则的作用域
#pragma pack(push, 1)
struct RpcHeader {
    uint16_t magic_num;         //魔数 (2B)
    uint32_t body_size;         //消息体长度 (4B)
    uint64_t seq_id;            //请求序列号 (8B，用于异步请求回调匹配)
    uint8_t msg_type;           //消息类型 MsgType (1B)
    uint8_t serialize_type;     //序列化类型 SerializeType (1B)
    uint32_t error_code;        //错误码 (4B, 0表示成功)
};
#pragma pack(pop)
//协议头总长：2 + 4 + 8 + 1 + 1 + 4 = 20 B

//完整的 RPC 消息对象 (面向开发者使用的逻辑对象)
class RpcMessage {
public:
    RpcHeader header;       //协议头
    std::string body;       //消息体 (已经过序列化、压缩、加密的二进制流)
    
    RpcMessage() {
        //初始化默认值
        header.magic_num = RPC_MAGIC_NUMBER;
        header.body_size = 0;
        header.seq_id = 0;
        header.msg_type = static_cast <uint8_t>(MsgType::REQUEST);
        header.serialize_type = static_cast<uint8_t>(SerializeType::PROTOBUF);
        header.error_code = 0;
    }
};

//协议编解码工具类
class RpcProtocol {
public:
    /**
     * @brief 封包 (Encode):将逻辑对象转化为可以通过网络传输的物理二进制流
     * @param msg 要发送的完整消息
     * @param out_buffer 输出的二进制流
     */
    static bool Encode(const RpcMessage& msg, std::string& out_buffer);

    /**
     * @brief 拆包 (Decode): 从网络接收到的物理二进制流中解析出逻辑对象
     * @param inbuffer 接收到的二进制流
     * @param out_msg 解析出的逻辑消息对象
     * @return 解析成功返回 true, 如果数据包不完整或者格式错误返回 false
     */
     static bool Decode(const std::string& in_buffer, RpcMessage& out_msg);
};

} //namespace mooncrpc