#pragma once

#include "../log/log_manager.h"
#include <zstd.h>

namespace mooncrpc {

//定义压缩级别枚举类
enum class Level {
    FASTEST = 1,    // 最快压缩速度，最低压缩率
    DEFAULT = 3,    // 默认级别，速度与压缩率的完美平衡
    BETTER = 7,     // 较高的压缩率，适合对带宽极度敏感的场景
    BEST = 19,      // 最佳压缩率，速度较慢，不推荐在实时 RPC 中使用
    MAX = 22        // 极限压缩，耗时极长
};

/**
 * @brief Zstd 压缩/解压工具类(单例)
 */
class ZstdCompress {
public:
    //单例模式
    static ZstdCompress& GetInstance() {
        static ZstdCompress instance;
        return instance;
    }

    //禁用拷贝和移动
    ZstdCompress(const ZstdCompress&) = delete;
    ZstdCompress& operator=(const ZstdCompress&) = delete;
    ZstdCompress(ZstdCompress&&) = delete;
    ZstdCompress& operator=(ZstdCompress&&) = delete;

    //字符串处理接口（通常用于 JSON 等文本序列化后的压缩）
    /**
     * @brief 压缩字符串
     * @param src 待压缩的源字符串
     * @param dst 压缩后的目标字符串(自动调整大小)
     * @param level 压缩级别，默认3级 Level::DEFAULT
     */
    bool CompressString(const std::string& src, std::string& dst, Level level = Level::DEFAULT);

    /**
     * @brief 解压字符串
     * @param src 待解压的源字符串
     * @param dst 解压后的目标字符串(自动调整大小)
     */
    bool DecompressString(const std::string& src, std::string& dst);

    //二进制流处理接口（通常用于 Protobuf 等二进制序列化的压缩）
    /**
     * @brief 压缩二进制数据流
     * @param src 源数据指针
     * @param src_len 源数据长度
     * @param dst 压缩后的目标容器(自动调整大小)
     * @param level 压缩级别，默认3级 Level::DEFAULT
     */
    bool CompressData(const char* src, size_t src_len, std::vector<char>& dst, Level level = Level::DEFAULT);

    /**
     * @brief 解压二进制数据流
     * @param src 源压缩数据指针
     * @param src_len 源数据长度
     * @param dst 解压后的目标容器(自动调整大小)
     * @param level 压缩级别，默认3级 Level::DEFAULT
     */
    bool DecompressData(const char* src, size_t src_len, std::vector<char>& dst);

private:
    ZstdCompress() = default;
    ~ZstdCompress() = default;
};

} //namespace mooncrpc