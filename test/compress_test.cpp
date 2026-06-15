#include "../src/log/log_manager.h"
#include "../src/compress_data/zstd_compress.h"
#include <iostream>
#include <string>

using namespace mooncrpc;

int main() {
    if (!Logger::GetInstance().Init("./test_logs")) {
        return -1;
    }

    LOG_INFO("====== Starting ZSTD Compression Module Test ======");

    // 1. 构造一份高度重复的测试数据（模拟包含大量数组的 JSON）
    std::string original_data = "{\"user_list\":[";
    for (int i = 0; i < 500; ++i) {
        original_data += "{\"id\":" + std::to_string(i) + ",\"name\":\"test_user\",\"role\":\"admin\"},";
    }
    original_data += "]}";

    LOG_INFO("Original data size: {} bytes", original_data.size());

    // 2. 测试压缩
    std::string compressed_data;
    auto& compressor = ZstdCompress::GetInstance();
    
    if (compressor.CompressString(original_data, compressed_data, Level::DEFAULT)) {
        LOG_INFO("Compression successful!");
        LOG_INFO("Compressed data size: {} bytes", compressed_data.size());
        
        double ratio = static_cast<double>(compressed_data.size()) / original_data.size() * 100.0;
        LOG_INFO("Compression Ratio: {:.2f}%", ratio);
    } else {
        LOG_ERROR("Compression failed!");
        return -1;
    }

    // 3. 测试解压
    std::string decompressed_data;
    if (compressor.DecompressString(compressed_data, decompressed_data)) {
        LOG_INFO("Decompression successful!");
        LOG_INFO("Decompressed data size: {} bytes", decompressed_data.size());
    } else {
        LOG_ERROR("Decompression failed!");
        return -1;
    }

    // 4. 核心校验：解压后的数据必须和原始数据完全一致
    if (original_data == decompressed_data) {
        LOG_INFO("Data integrity check PASSED: Decompressed data perfectly matches original data.");
    } else {
        LOG_FATAL("Data integrity check FAILED: Data mismatch!");
    }

    LOG_INFO("====== ZSTD Module Test Completed Successfully ======");
    return 0;
}