#pragma once
//实现线程池的配置

#include "../log/log_manager.h"
#include <cstdint>
#include <nlohmann/json.hpp>

namespace mooncrpc {

class ThreadPoolConfig {    //负责存储解析后的线程池参数
public:
    ThreadPoolConfig() = default;
    ~ThreadPoolConfig() = default;

    //从JSON对象初始化
    bool InitThreadPoolConfig(const nlohmann::json& config);

    //Getter方法
    uint16_t GetCoreThreads() const { return core_threads_; }
    uint16_t GetMaxThreads() const { return max_threads_; }
    uint32_t GetQueueSize() const { return queue_size_; }
    uint32_t GetKeepAliveTime() const { return keep_alive_time_; }

    //Setter方法
    void SetCoreThreads(uint16_t core_threads) { core_threads_ = core_threads; }
    void SetMaxThreads(uint16_t max_threads) { max_threads_ = max_threads; }
    void SetQueueSize(uint32_t queue_size) { queue_size_ = queue_size; }
    void SetKeepAliveTime(uint32_t keep_alive_time) { keep_alive_time_ = keep_alive_time; }
    
    //禁用拷贝和移动
    ThreadPoolConfig(const ThreadPoolConfig&) = delete;
    ThreadPoolConfig& operator=(const ThreadPoolConfig&) = delete;
    ThreadPoolConfig(ThreadPoolConfig&&) = delete;
    ThreadPoolConfig& operator=(ThreadPoolConfig&&) = delete;

private:
    uint16_t core_threads_ = 0;     //核心线程数
    uint16_t max_threads_ = 0;      //最大线程数
    uint32_t queue_size_ = 0;       //任务队列大小
    uint32_t keep_alive_time_ = 0;  //空闲线程存活时间(s)[约束非核心线程]
};

}
