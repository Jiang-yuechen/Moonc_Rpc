#include "aes_encrypt.h"
#include "log/log_manager.h"
#include <algorithm>
#include <random>

namespace mooncrpc {

namespace {
    //Base64 字典表
    const std::string BASE64_CHARS = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    //生成高质量的随机字节序列(作为会话密钥)
    std::string GenerateRandomBytes(size_t length) {
        std::string result;
        result.reserve(length);

        // std::random_device rd;
        // std::mt19937 gen(rd());
        
        // 使用 thread_local 保证多线程安全并且利用 Lambda 表达式确保引擎只在线程首次调用时初始化一次
        thread_local std::mt19937 gen([]() {
            std::random_device rd;
            return rd();
        }());

        // 分布器很轻量可以每次创建
        std::uniform_int_distribution<> dis(0, 255);

        for (size_t i = 0; i < length; i++) {
            result.push_back(static_cast<char>(dis(gen)));
        }
        return result;
    }

    // 获取当前时间戳（ms）
    uint64_t GetCurrentTimestampMs() {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    }
}

//构造函数：初始化写死的主密钥(在生产环境中可以从配置文件中读取)
AesEncrypt::AesEncrypt() {
    master_key_ = "MooncRPC_Secret_Key_2026_Prod!@#$";
}

/**
 * @brief Base64 编码实现
 * @details 算法核心原理：
 * 把每 3 个字节(24bit)的二进制数据重新切分为 4 组，每组 6 bit。
 * 因为 6bit 最多能表示64种状态，正好可以映射到一个包含 64 个字符的字典表 (BASE64_CHARS) 中。
 */
std::string AesEncrypt::Base64Encode(const std::string& input) const {
    std::string ret;
    int i = 0;  //记录当前处理到了 3 字节分组的第几个字节
    int j = 0;  //辅助循环变量

    // char_array_3 暂存 3 个原始字节(共 24 bits)
    unsigned char char_array_3[3];
    // char_array_4 暂存拆分后的 4 个 6bit 单元的索引值(共 24 bits)
    // 作为索引时可以将 2 进制值转换成 10 进制去查表
    unsigned char char_array_4[4];

    //强转成无符号字符指针，防止位操作时最高位的符号位产生干扰
    const unsigned char* bytes_to_encode = reinterpret_cast<const unsigned char*>(input.data());
    size_t input_len = input.length();

    //逐字节读取，直到把输入字符串读取完
    while (input_len--) {
        char_array_3[i++] = *(bytes_to_encode++);   //把字节塞进3字节数组

        //凑齐 3 字节后开始进行 24-bit 拆分成 4 份的操作
        if (i == 3) {   //这里仅char_array_3[0] ~ char_array_3[2] 有数据
            //1. 生成第一个 6 位：
            //取第 1 个字节的前 6 位，跟 0xfc(11111100) 按位与后 右移两位
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;

            //2. 生成第 2 个 6 位：
            //取第 1 个字节的后 2 位：和 (0x03 -> 00000011) 按位与后 左移 4 bit
            //加上第 2 个字节的前 4 位： 和(0xf0 -> 11110000) 按位与后 右移 4 bit
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);

            //3. 生成第 3 个 6 位：
            //取第 2 个字节的后 4 位：和 (0x0f -> 00001111) 按位与后 左移 2 位
            //加上第 3 个字节的前 2 位：和(0xc0 -> 11000000) 按位与后 右移 6 位
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

            //4. 生成第 4 个 6 位：
            //取第 3 个字节的最后 6 位：和 (0x3f -> 00111111) 按位与
            char_array_4[3] = char_array_3[2] & 0x3f;

            //把算出的 4 个索引值去字典表中查出对应的字符拼接到结果字符串中
            for (int i = 0; i < 4; i++) {
                ret += BASE64_CHARS[char_array_4[i]];
            }
            
            i = 0;  //重置计数器，准备接收下 3 个字节数据
        }
    }

    //当输入字符串长度不是 3 整数倍时会剩 1 ~ 2 字节数据
    if (i) {
        //先用 '\0' 把不足 3B 的部分填满，防止脏数据干扰
        for (int j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        //重新计算前几个 6 位组合
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        //只把真实数据部分拼接到结果里(i 剩下 1 就拼 2 个字符，剩 2 就拼 3 个字符)
        for (int j = 0; j < i + 1; j++) {
            ret += BASE64_CHARS[char_array_4[j]];
        }

        //不足的部分统一用 '=' 填充对齐(Base64 标准规范：以 4 个字符作为一组输出)
        while (i++ < 3) {
            ret += '=';
        }
    }
    return ret;
}

std::string AesEncrypt::Base64Decode(const std::string& input) const {
    std::string ret;

    //构建 base64 反向查询字典表，根据字符查出对应在 BASE64_CHARS中的索引
    //大小为 256 覆盖了所有 ASCII 码
    std::vector<int> base64_map(256, -1);   
    for (size_t i = 0; i < BASE64_CHARS.size(); i++) {
        base64_map[BASE64_CHARS[i]] = i;
    }

    size_t input_len = input.size();
    int i = 0;          //记录当前收到了 4 个字符分组中的第几个字符
    int j = 0;          //辅助变量
    int curr = 0;        //记录当前遍历到输入字符串的哪个位置

    //char_array_4:暂存读到的 4 个 Base64 字符
    unsigned char char_array_4[4];
    //char_array_3:暂存还原出来的 3 个原始字节
    unsigned char char_array_3[3];
    
    //不断从 input 读取编码字符，读完或者遇到 '=' 就停止
    while (input_len-- && input[curr] != '=') {
        char_array_4[i++] = input[curr];
        curr++;
        
        //凑齐了 4 个字符后就进行 4 * 6 bits -> 3 * 8 bits 的反向拼装
        if (i == 4) {
            //通过刚才建立的字典将读取的 4 个 ASCII 字符转换成对应的真实数据 (0~63)
            //char_array_4 中装的只有后 6 位是有效的数据 (00xxxxxx)
            for (int i = 0; i < 4; i++) {
                char_array_4[i] = base64_map[char_array_4[i]];
            }

            // 还原第 1 个原始字节
            // 取第 1 个字符的全部 6 位，左移 2 位 (腾出尾部空间)
            // 取第 2 个字符的前 2 位 (0x30 -> 110000)，右移 4 位填补进去
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);

            // 还原第 2 个原始字节
            // 取第 2 个字符的后 4 位 (0x0f -> 001111)，左移 4 位 (腾出尾部空间)
            // 取第 3 个字符的前 4 位 (0x3c -> 111100)，右移 2 位填补进去
            char_array_3[1] = ((char_array_4[1] & 0x0f) << 4) + ((char_array_4[2] & 0x3c) >> 2);

            // 还原第 3 个原始字节
            // 取第 3 个字符的后 2 位 (0x03 -> 000011)，左移 6 位
            // 取第 4 个字符的全部 6 位直接填进去
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            //把 3 个拼装好的原始字节放入结果字符串中
            for (int i = 0; i < 3; i++) {
                ret += char_array_3[i];
            }

            //重置计数器，处理下一组的 4 个字符
            i = 0;
        }
    }

    //尾部处理:输入字符串不够，最后不足 4 个字符
    if (i) {
        //先处理脏数据，用 0 填充
        for (j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }

        //把字符转成 0 ~ 63 的真实索引
        for (j = 0; j < 4; j++) {
            char_array_4[j] = base64_map[char_array_4[j]];
        }
    
        //继续重新拼装成真实数据
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0x0f) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
    
        //剔除垃圾数据
        // 如果剩下 2 个字符 (i=2)，只能还原出 1 个原始字节 (2-1=1)
        // 如果剩下 3 个字符 (i=3)，只能还原出 2 个原始字节 (3-1=2)
        for (int j = 0; j < i - 1; j++) {
            ret += char_array_3[j];
        }
    }
    return ret;
}

/**
 * @brief 核心移位加密：字符值 + 密钥值 + 偏移量
 * @param input 待加密数据
 * @param key 加密密钥
 * @param return 加密后的数据
 * @details 算法设计：
 * - 位置相关加密：每个字符的加密依赖其位置
 * - 多重混淆：结合字符值、密钥字符和位置偏移
 * - 循环密钥：密钥长度可小于数据长度
 * 加密公式：encrypted[i] = (data[i] + key[i % key_len] + i % 256) % 256
 */
std::string AesEncrypt::ShiftEncrypt(const std::string& input, const std::string& key) const {
    std::string result;
    result.reserve(input.size());   //预留空间
 
    for (size_t i = 0; i < input.size(); i++) {
        unsigned char c = input[i];             //原始字符
        unsigned char k = key[i % key.size()];  //循环使用密钥
        unsigned char shift = (i % 256);        //位置相关的偏移量
 
        //三重混合：字符 + 密钥 + 位置偏移
        result.push_back(static_cast<char>((c + k + shift) % 256));
    }
    return result;
}

/**
 * @brief 核心移位解密：加密的逆操作
 * @param input 待解密数据
 * @param key 解密密钥(和加密时相同)
 * @return 解密后的原始数据
 * @details 算法原理：
 * - 减法操作逆转加密过程
 * - +512 避免负数取模问题
 * 解密公式：decrypted[i] = (encrypted[i] - key[i % key_len] - i % 256 + 512) % 256
 */
std::string AesEncrypt::ShiftDecrypt(const std::string& input, const std::string& key) const {
    std::string result;
    result.reserve(input.size());
    
    for (size_t i = 0; i < input.size(); i++) {
        unsigned char c = input[i];
        unsigned char k = key[i % key.size()];
        unsigned char shift = i % 256;
        result.push_back(static_cast<char>((c - k - shift + 512) % 256));
    }
    return result;
}

/**
 * @brief 加密主函数：实现双重加密机制
 * @param data 待加密原始数据
 * @param ciphertext 输出参数：存储 Base64 编码的密文
 * @details 加密流程：
 * 1. 生成随机会话密钥(32B)
 * 2. 使用会话密钥加密数据 -> encrypted_data
 * 3. 使用主密钥加密会话密钥 -> encrypted_key
 * 4. 组合：[encrypted_key][encrypted_data]
 * 5. Base64 编码最终结果
 * 安全性分析：
 * - 每次加密使用不同的随机会话密钥
 * - 及时数据相同，每次加密后结果也不同
 * - 攻击者无法直接获取会话密钥(被主密钥保护)
 */
bool AesEncrypt::Encrypt(const std::string& data, std::string& ciphertext) {
    try {
        // 业务层返回 0 字节包是合法的
        // if (data.empty()) {
        //     LOG_ERROR("Input data is empty.");
        //     return false;
        // }

        //1. 生成 32 字节的随机动态会话密钥
        std::string session_key = GenerateRandomBytes(KEY_LENGTH);

        // 防重放攻击：将 8 字节的当前时间戳拼接到明文最前面
        uint64_t current_ts = GetCurrentTimestampMs();
        std::string ts_prefix(sizeof(uint64_t), '\0');      // 时间戳前缀初始化
        // 将时间戳的内存拷贝到字符串前缀中
        std::memcpy(&ts_prefix[0], &current_ts, sizeof(uint64_t));
        
        std::string data_with_ts = ts_prefix + data;;

        //2. 用会话密钥加密 时间戳 + 真实的 Payload 数据
        std::string encrypted_data = ShiftEncrypt(data_with_ts, session_key);

        //3. 用主密钥对会话密钥进行加密
        std::string encrypted_key = ShiftEncrypt(session_key, master_key_);

        //4. 将加密后的密钥和加密后的数据拼接起来，并做 Base64 编码
        std::string combined = encrypted_key + encrypted_data;
        ciphertext = Base64Encode(combined);

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Encryption failed: {}", e.what());
        return false;
    }
}

/**
 * @brief 解密主函数：双重解密机制
 * @param encrypted_data Base64 编码的密文
 * @param plaintext 输出参数：存储解密后的明文
 * @details 解密流程：
 * 1. Base64 解码获取二进制密文
 * 2. 分离前 32 字节(加密的会话密钥) 和剩余部分(加密的数据)
 * 3. 使用主密钥解密会话密钥
 * 4. 使用解密后的会话密钥解密数据
 * 错误处理：
 * - 验证解码后数据长度(必须大于 32B)
 * - 捕获所有可能的异常并记录日志
 */
bool AesEncrypt::Decrypt(const std::string& encrypted_data, std::string& plaintext) {
    try {
        //1. Base64 解码恢复二进制流
        std::string decoded = Base64Decode(encrypted_data);

        //2. 数据防破坏校验：总长度必须大于一个密钥的长度
        if (decoded.size() <= KEY_LENGTH) {
            LOG_ERROR("Decoded data too short: {} bytes, it should > {} bytes", decoded.size(), KEY_LENGTH);
            return false;
        }

        //3. 分离出加密后的密钥和加密后的数据
        std::string encrypted_key = decoded.substr(0, KEY_LENGTH);      //前 32B
        std::string encrypted_data_part = decoded.substr(KEY_LENGTH);     //剩余部分

        //4. 用主密钥解密会话密钥，再用会话密钥解密数据得到最终明文
        std::string session_key = ShiftDecrypt(encrypted_key, master_key_);
        std::string decrypted_data_with_ts = ShiftDecrypt(encrypted_data_part, session_key);
        
        // 防重放攻击：提取解密后的时间戳并进行阈值校验
        if (decrypted_data_with_ts.size() < sizeof(uint64_t)) {
            LOG_ERROR("Decrypted data corrupted: missing timestamp header.");
            return false;   // 直接放弃
        }

        uint64_t received_ts = 0;
        std::memcpy(&received_ts, decrypted_data_with_ts.data(), sizeof(uint64_t));

        uint64_t current_ts = GetCurrentTimestampMs();

        // 设定阈值：如果时间差超过 5000 ms，或者时间戳来自未来（防止时间篡改）直接放弃
        if (current_ts < received_ts || (current_ts - received_ts) > 5000) {
            LOG_ERROR("Replay attack detected or packet expired! Time diff exceeds 5000ms.");
            return false;
        }

        // 校验通过：剔除前 8 字节的时间戳，返回纯净的业务明文
        plaintext = decrypted_data_with_ts.substr(sizeof(uint64_t));

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Decryption failed: {}", e.what());
        return false;
    }
}
    
} // namespace mooncrpc