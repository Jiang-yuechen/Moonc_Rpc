#pragma once
#include <string>
#include <vector>
#include <memory>

namespace mooncrpc {

/**
 * @brief 安全加密工具(单例模式)
 * @details 采用双重加密机制：主密钥加密动态生成的会话密钥，会话密钥加密实际数据。
 * 最后使用 Base64 编码，确保二进制密文可以在任何网络协议中安全传输。
 */
class AesEncrypt {
public:
    //单例模式：获取全局唯一实例
    static AesEncrypt& GetInstance() {
        static AesEncrypt instance;
        return instance;
    }

    //核心接口：加密数据
    bool Encrypt(const std::string& data, std::string& ciphertext);

    //核心接口：解密数据
    bool Decrypt(const std::string& encrypted_data, std::string& plaintext);

    //禁用拷贝和移动
    AesEncrypt(const AesEncrypt&) = delete;
    AesEncrypt& operator=(const AesEncrypt&) = delete;
    AesEncrypt(AesEncrypt&&) = delete;
    AesEncrypt& operator=(AesEncrypt&&) = delete;

private:
    AesEncrypt();
    ~AesEncrypt() = default;

    //自实现的 Base64 编解码(避免引入外部庞大的依赖库)
    /**
     * @brief Base64 编码
     * @param input 待编码的二进制数据
     * @return 编码后的 Base64 字符串
     */
    std::string Base64Encode(const std::string& input) const;
    
    /**
     * @brief Base64 解码
     * @param input Base64 编码的字符串
     * @return 解码后的二进制数据
     */
    std::string Base64Decode(const std::string& input) const;

    //核心移位加解密算法
    /**
     * @brief 移位加密算法
     * @param input 待加密数据
     * @param key 加密密钥
     */
    std::string ShiftEncrypt(const std::string& input, const std::string& key) const;

    /**
     * @brief 移位解密算法
     * @param input 待解密的数据
     * @param key 解密密钥 */
    std::string ShiftDecrypt(const std::string& input, const std::string& key) const;

    //成员变量
    std::string master_key_;                //主密钥
    static constexpr size_t KEY_LENGTH = 32;//会话密钥长度(32字节)
};

} //namespace mooncrpc