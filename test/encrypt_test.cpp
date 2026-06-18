#include "../src/encrypt/aes_encrypt.h"
#include "../src/log/log_manager.h"
#include <iostream>
#include <string>

using namespace mooncrpc;

int main() {
    if (!Logger::GetInstance().Init("./test_logs")) {
        return -1;
    }

    LOG_INFO("====== Starting Encrypt Module Test ======");

    // 1. 准备一段非常敏感的原始数据
    std::string original_data = "MooncRPC Highly Sensitive User Data: password123456";
    LOG_INFO("Original Data: {}", original_data);

    // 2. 加密测试
    std::string ciphertext;
    auto& encryptor = AesEncrypt::GetInstance();

    if (encryptor.Encrypt(original_data, ciphertext)) {
        LOG_INFO("Encryption Successful!");
        LOG_INFO("Ciphertext (Base64): {}", ciphertext);
    } else {
        LOG_ERROR("Encryption Failed!");
        return -1;
    }

    // 3. 动态密钥特性验证：再次加密同样的明文，观察密文是否不同
    std::string ciphertext_second;
    encryptor.Encrypt(original_data, ciphertext_second);
    if (ciphertext != ciphertext_second) {
        LOG_INFO("Dynamic Session Key Check PASSED: Same plaintext produced different ciphertext.");
    }

    // 4. 解密测试
    std::string decrypted_data;
    if (encryptor.Decrypt(ciphertext, decrypted_data)) {
        LOG_INFO("Decryption Successful!");
        LOG_INFO("Decrypted Data: {}", decrypted_data);
    } else {
        LOG_ERROR("Decryption Failed!");
        return -1;
    }

    // 5. 完整性校验
    if (original_data == decrypted_data) {
        LOG_INFO("Data Integrity Check PASSED: Decrypted text perfectly matches original.");
    } else {
        LOG_FATAL("Data Integrity Check FAILED!");
        return -1;
    }

    LOG_INFO("====== Encrypt Module Test Completed Successfully ======");
    return 0;
}