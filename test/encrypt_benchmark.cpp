#include "log/log_manager.h"
#include "encrypt/aes_encrypt.h"
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <thread>

using namespace mooncrpc;

int main() {
    // 1. 初始化日志系统
    if (!Logger::GetInstance().Init("../config/log_config.json")) {
        return -1;
    }

    CLIENT_INFO("====== Starting Encryption Benchmark & Security Test ======");

    auto& encryptor = AesEncrypt::GetInstance();

    // 2. 构造测试数据：模拟一个约 1KB 的真实 RPC JSON/Protobuf 序列化包
    std::string base_payload = "{\"method\":\"GetUserInfo\",\"params\":{\"user_id\":10086,\"role\":\"admin\"},\"timestamp\":1718611200}";
    std::string payload;
    for (int i = 0; i < 10; i++) {
        payload += base_payload; 
    }
    CLIENT_INFO("Test payload size: {} bytes", payload.size());

    const int ITERATIONS = 10000;       // 迭代次数：10000 次
    CLIENT_INFO("Target iterations: {}", ITERATIONS);

    // 预分配内存保存密文，供后续解密测试和防重放测试使用
    std::vector<std::string> ciphertexts(ITERATIONS); 

    // ---------------------------------------------------------
    // Phase 1: 加密性能基准测试 (10000 次)
    // ---------------------------------------------------------
    CLIENT_INFO("--- Phase 1: Encryption Benchmark ---");
    auto encrypt_start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < ITERATIONS; i++) {
        if (!encryptor.Encrypt(payload, ciphertexts[i])) {
            CLIENT_ERROR("Encryption failed at iteration {}", i);
            return -1;
        }
    }
    
    auto encrypt_end = std::chrono::high_resolution_clock::now();
    auto encrypt_duration = std::chrono::duration_cast<std::chrono::milliseconds>(encrypt_end - encrypt_start).count();

    // ---------------------------------------------------------
    // Phase 2: 解密性能基准测试 (10000 次)
    // ---------------------------------------------------------
    CLIENT_INFO("--- Phase 2: Decryption Benchmark ---");
    std::string decrypted_text;
    auto decrypt_start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < ITERATIONS; i++) {
        if (!encryptor.Decrypt(ciphertexts[i], decrypted_text)) {
            CLIENT_ERROR("Decryption failed at iteration {}", i);
            return -1;
        }
    }
    
    auto decrypt_end = std::chrono::high_resolution_clock::now();
    auto decrypt_duration = std::chrono::duration_cast<std::chrono::milliseconds>(decrypt_end - decrypt_start).count();

    // ---------------------------------------------------------
    // Phase 3: 性能数据出具
    // ---------------------------------------------------------
    CLIENT_INFO("--- Performance Report ---");
    CLIENT_INFO("Total Encrypt Time for {} ops: {} ms", ITERATIONS, encrypt_duration);
    CLIENT_INFO("Average Encrypt Time: {:.4f} ms/op", static_cast<double>(encrypt_duration) / ITERATIONS);
    CLIENT_INFO("Total Decrypt Time for {} ops: {} ms", ITERATIONS, decrypt_duration);
    CLIENT_INFO("Average Decrypt Time: {:.4f} ms/op", static_cast<double>(decrypt_duration) / ITERATIONS);

    // ---------------------------------------------------------
    // Phase 4: 防重放攻击 (Replay Attack) 实战验证
    // ---------------------------------------------------------
    CLIENT_INFO("--- Phase 4: Replay Attack Prevention Test ---");
    
    // 修复：现场生成一个全新的密文，避免超时
    std::string fresh_ciphertext;
    if (!encryptor.Encrypt("fresh_test_payload", fresh_ciphertext)) {
        CLIENT_ERROR("Failed to generate fresh ciphertext for testing.");
        return -1;
    }
    
    CLIENT_INFO("Testing immediate decryption (should succeed)...");
    std::string temp_decrypted;
    // 此时解密，时间差通常在 1 毫秒以内，绝对不会触发 5000ms 的过期机制
    if (encryptor.Decrypt(fresh_ciphertext, temp_decrypted)) {
        CLIENT_INFO("Immediate decryption: SUCCESS");
    } else {
        CLIENT_ERROR("Immediate decryption: FAILED");
    }

    CLIENT_INFO("Sleeping for 6 seconds to trigger expiration threshold (5000ms)...");
    std::this_thread::sleep_for(std::chrono::seconds(6));
    
    CLIENT_INFO("Attempting to replay the exact same packet from 6 seconds ago...");
    std::string expired_decrypted;
    
    // 预期结果：因为已经睡眠了 6 秒，此时解密必定被拦截，返回 false
    if (!encryptor.Decrypt(fresh_ciphertext, expired_decrypted)) {
        CLIENT_INFO("Replay/Expiration check PASSED: Expired packet was correctly rejected by security layer.");
    } else {
        CLIENT_FATAL("Replay/Expiration check FAILED: Expired packet was somehow accepted!");
        return -1;
    }

    CLIENT_INFO("====== Benchmark & Security Test Completed Successfully ======");
    return 0;
}