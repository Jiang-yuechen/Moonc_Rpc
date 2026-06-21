#include "../log/log_manager.h"
#include "rpc_protocol.h"
#include <cstring>  //std::memcpy

namespace mooncrpc {

constexpr size_t HEADER_SIZE = sizeof(RpcHeader);  //协议头长度

bool RpcProtocol::Encode(const RpcMessage& msg, std::string& out_buffer) {
    try {
        //1.计算总长度 = 包头长度 + 包体长度
        size_t total_size = HEADER_SIZE + msg.body.size();
        out_buffer.resize(total_size);

        //2.为了保证安全性，拷贝前更新 body_size 为实际大小
        RpcHeader header_to_send = msg.header;
        header_to_send.body_size = static_cast<uint32_t>(msg.body.size());

        //3.将 Header 拷贝到 buffer 最前面 (内存连续拷贝)
        // data() 方法返回指向字符串首地址的指针
        std::memcpy(out_buffer.data(), &header_to_send, HEADER_SIZE);

        //4.将 Body 拷贝到 Header 之后 (如果消息体有内容)
        if (header_to_send.body_size > 0) {
            std::memcpy(out_buffer.data() + HEADER_SIZE, msg.body.data(), header_to_send.body_size);
        }

        LOG_DEBUG("Protocol Encode SUCCESS. Total size: {} bytes (Header: {}, Body: {})",
            total_size, Header_size, header_to_send.body_size);
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Protocol Encode exception: {}", e.what());
        return false;
    }
}

bool RpcProtocol::Decode(const std::string& in_buffer, RpcMessage& out_msg) {
    try {
        //1.协议头半包校验：如果接收到的数据连包头大小都不够，直接拒绝解析，等待更多数据到来
        if (in_buffer.size() < HEADER_SIZE) {
            LOG_WARN("Decode FAILED: Buffer size ({}) is smaller than header size ({})", 
                    in_buffer.size(), HEADER_SIZE);
            return false;
        }

        //2.提取 Header (内存连续拷贝出来)
        RpcHeader parsed_header;
        std::memcpy(&parsed_header, in_buffer.data(), HEADER_SIZE);

        //3.魔数校验：若魔数失配说明有人发了恶意数据或者协议串网了
        if (parsed_header.magic_num != RPC_MAGIC_NUMBER) {
            LOG_ERROR("Decode FAILED: Magic Number mismatch! Expected: {:X}, Got: {:X}",
                    RPC_MAGIC_NUMBER, parsed_header.magic_num);
        }

        //4.数据半包校验:若当前缓冲区中的数据总长度还没达到包头里声明的长度，说明数据还没发送完
        size_t expected_total_size = HEADER_SIZE + parsed_header.body_size;
        if (in_buffer.size() < expected_total_size) {
            LOG_WARN("Decode INCOMPLETE: Need {} bytes, but only got {} bytes. Waiting for more network data.");
            return false;
        }

        //5.数据完整，开始真正装载数据
        out_msg.header = parsed_header;
        if (parsed_header.body_size > 0) {
            //分配从(in_buffer.data() + HEADER_SIZE)开始的(parsed_header.body_size)长度数据到out_msg.body
            out_msg.body.assign(in_buffer.data() + HEADER_SIZE, parsed_header.body_size);
        }

        LOG_DEBUG("Protocol Decode SUCCESS. SeqId: {}, Body size: {}",
                parsed_header.seq_id, parsed_header.body_size);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Protocol Decode exception: {}", e.what());
        return false;
    }
}

} //namespace mooncrpc