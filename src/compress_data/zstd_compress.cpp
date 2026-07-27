#include "zstd_compress.h"
#include "log/log_manager.h"

namespace mooncrpc {

bool ZstdCompress::CompressString(const std::string& src, std::string& dst, Level level) {
    if (src.empty()) {
        dst.clear();
        return true;
    }

    //1.获取压缩后的理论最大体积，用于预分配内存
    size_t const compressed_bound = ZSTD_compressBound(src.size());
    dst.resize(compressed_bound);

    //2.执行 Zstd压缩
    size_t const actual_size = ZSTD_compress(
        dst.data(), compressed_bound,   //目标缓冲区及容量
        src.data(), src.size(),         //源数据及大小
        static_cast<int>(level)     //压缩级别
    );

    //3.检查是否发生错误
    if (ZSTD_isError(actual_size)) {
        LOG_ERROR("ZSTD string compression failed: {}", ZSTD_getErrorName(actual_size));
        return false;
    }

    //4.将目标字符串调整到实际压缩后大小
    dst.resize(actual_size);
    return true;
}

bool ZstdCompress::DecompressString(const std::string& src, std::string& dst) {
    if(src.empty()) {
        dst.clear();
        return true;
    }

    //1.从压缩帧中读取原始未压缩时的数据大小
    unsigned long long const decompressed_size = ZSTD_getFrameContentSize(src.data(), src.size());

    //获取失败处理(数据损坏或格式错误)
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        LOG_ERROR("ZSTD string decompression failed: Unable to get original size.");
        return false;
    }
    dst.resize(decompressed_size);

    //2.执行 Zstd 解压缩
    size_t const actual_size = ZSTD_decompress(
        dst.data(), decompressed_size, 
        src.data(), src.size()
    );

    //解压失败
    if (ZSTD_isError(actual_size)) {
        LOG_ERROR("ZSTD string decompression failed: {}", ZSTD_getErrorName(actual_size));
        return false;
    }

    dst.resize(actual_size);
    return true;
}

bool CompressData(const char* src, size_t src_len, std::vector<char>& dst, Level level) {
    if (!src || src_len == 0) {
        dst.clear();
        return true;
    }

    //1.获取压缩后的理论最大体积，用于预分配内存
    size_t const compressed_bound = ZSTD_compressBound(src_len);
    dst.resize(compressed_bound);

    //2.执行 Zstd压缩
    size_t const actual_size = ZSTD_compress(
        dst.data(), compressed_bound,   //目标缓冲区及容量
        src, src_len,         //源数据及大小
        static_cast<int>(level)     //压缩级别
    );

    //3.检查是否发生错误
    if (ZSTD_isError(actual_size)) {
        LOG_ERROR("ZSTD binary compression failed: {}", ZSTD_getErrorName(actual_size));
        return false;
    }

    //4.将目标字符串调整到实际压缩后大小
    dst.resize(actual_size);
    return true;
}

bool DecompressData(const char* src, size_t src_len, std::vector<char>& dst) {
    if(!src || src_len == 0) {
        dst.clear();
        return true;
    }

    //1.从压缩帧中读取原始未压缩时的数据大小
    unsigned long long const decompressed_size = ZSTD_getFrameContentSize(src, src_len);

    //获取失败处理(数据损坏或格式错误)
    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        LOG_ERROR("ZSTD binary decompression failed: Unable to get original size.");
        return false;
    }
    dst.resize(decompressed_size);

    //2.执行 Zstd 解压缩
    size_t const actual_size = ZSTD_decompress(
        dst.data(), decompressed_size, 
        src, src_len
    );

    //解压失败
    if (ZSTD_isError(actual_size)) {
        LOG_ERROR("ZSTD binary decompression failed: {}", ZSTD_getErrorName(actual_size));
        return false;
    }

    dst.resize(actual_size);
    return true;
}

} //namespace mooncrpc
