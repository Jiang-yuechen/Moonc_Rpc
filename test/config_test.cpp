#include "../src/load_config/thread_pool_config.h"
#include "../src/log/log_manager.h"
#include <iostream>

using namespace mooncrpc;
using json = nlohmann::json;

int main() {
    //1.初始化日志模块
    if (!Logger::GetInstance().Init("./test_logs")) {
        std::cerr << "初始化日志失败！" << std::endl;
        return -1;
    }

    LOG_INFO("======开始线程池配置测试======");
    //2.模拟从配置文件中读取数据，避免依赖外部文件读取逻辑
    json mock_config = {
        { "core_threads", 8 },
        { "max_threads", 16 },
        { "queue_size", 10000 },
        { "keep_alive_time", 60 }
    };

    //3.创建配置对象并初始化
    ThreadPoolConfig thread_pool_config;
    if (thread_pool_config.InitThreadConfig(mock_config)) {
        LOG_INFO("线程池配置解析成功！");

        //验证解析结果
        LOG_INFO("Core Threads: {}", thread_pool_config.GetCoreThreads());
        LOG_INFO("Max Threads: {}", thread_pool_config.GetMaxThreads());
        LOG_INFO("Queue Threads: {}", thread_pool_config.GetCoreThreads());
        LOG_INFO("Keep Alive Time: {}s", thread_pool_config.GetKeepAliveTime());
    } else {
        LOG_ERROR("线程池配置解析失败！");
    }

    //4.异常测试
    LOG_INFO("======测试异常配置======");
    json invalid_config = {
        { "core_threads", 0}
    };
    ThreadPoolConfig invalid_cr_config;
    if (!invalid_cr_config.InitThreadConfig(invalid_config)) {
        LOG_INFO("已重新调整为合理配置");
    } else {
        LOG_ERROR("重新调整配置数据失败！");
    }

    LOG_INFO("===线程池配置数据测试结束===");

    return 0;
}