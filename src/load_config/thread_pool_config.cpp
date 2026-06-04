#include "thread_pool_config.h"

namespace mooncrpc {

bool ThreadPoolConfig::InitThreadConfig(const nlohmann::json& config) {
    try {
        //使用config.value("key", default_value) 避免大量繁琐的if else语句
        //当不能从config读取到相应的键值时会启用替补参数default_value传给变量
        //配置核心线程数
        uint16_t core_threads = config.value("core_threads", 4);
        if (core_threads == 0) {
            LOG_ERROR("Invalid core_threads: 核心线程数不应该为0");
            return false;
        }
        SetCoreThreads(core_threads);   //正常应该读取到自己json文件中的内容
        
        //配置最大线程数
        uint16_t max_threads = config.value("max_threads", 10);
        if (max_threads < core_threads) {
            LOG_ERROR("Invalid max_threads: 最大线程数应大于核心线程数");
            return false;
        }
        SetMaxThreads(max_threads);

        //配置任务队列数
        uint32_t queue_size = config.value("queue_size", 100);
        SetQueueSize(queue_size);

        //配置空闲线程存活时间
        uint32_t keep_alive_time = config.value("keep_alive_time", 60);
        SetKeepAliveTime(keep_alive_time);

        LOG_INFO("线程池配置成功！");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("初始化线程池配置失败: {}", e.what());
        return false;
    }
}

}   //namespace mooncrpc