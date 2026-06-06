#include "../src/log/log_manager.h"
#include <thread>
#include <chrono>

int main() {
    //1.初始化日志模块
    if (!mooncrpc::Logger::GetInstance().Init()) {
        return -1;
    }
    
    //2.测试各种级别宏定义输出
    LOG_INFO("MooncRPC Logger initialized!");
    LOG_DEBUG("This is a debug message. Vars:{}, {}", 42, "hello");
    LOG_WARN("This is a warn message.");

    //3.测试跨线程打印
    std::thread t([]() {
        LOG_ERROR("Error form another thread.");
    });

    t.join();

    return 0;
}