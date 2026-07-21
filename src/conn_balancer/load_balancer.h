#pragma once
#include <random>
#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <functional>

namespace mooncrpc {

//负载均衡基类接口
class LoadBalancer {
public:
    virtual ~LoadBalancer() = default;

    //核心接口：挑选可用节点(纯虚函数)
    virtual std::string select(const std::vector<std::string>& instance) = 0;

    // 为了让多态下的子类能够接受权重配置，基类预留虚拟接口
    virtual void initWeights(const std::unordered_map<std::string, int>& weights) {}

    // 单例与工厂对外接口
    static std::shared_ptr<LoadBalancer> getInstance();
    static bool initBalancer(const std::string& type);
    static std::string selectServer(const std::vector<std::string>& servers);

protected:
    static std::mutex mutex_;
    static std::shared_ptr<LoadBalancer> instance_;
};

// 1. 随机负载均衡器实现
class RandomLoadBalancer : public LoadBalancer {
public:
    std::string select(const std::vector<std::string>& instance) override;
};

// 2. 轮询负载均衡器
class RoundRobinLoadBalancer : public LoadBalancer {
public:
    RoundRobinLoadBalancer() : current_index_(0) {}

    // std::string Select(const std::vector<std::string>& nodes) override {
    //     if (nodes.empty()) {
    //         return "";
    //     }

    //     // fetch_add 是原子操作，先获取旧值再 add
    //     // 1：增量
    //     // memory_order_relaxed 表示宽松内存序，不要求强烈的内存屏障，提高性能
    //     size_t current_index = index_.fetch_add(1, std::memory_order_relaxed);

    //     // 取模让指针循环回到头部
    //     return nodes[current_index % nodes.size()];
    // }
    std::string select(const std::vector<std::string>& instances) override;

private:
    std::atomic<size_t> current_index_;     // 原子计数器替代普通指针
};

// 3. 加权轮询负载均衡器
class WeightedRoundRobinLoadBalancer : public LoadBalancer {
public:
    WeightedRoundRobinLoadBalancer() : current_index_(0) {}
    
    // 从配置初始化权重
    void initWeights(const std::unordered_map<std::string, int>& weights) override {
        weights_ = weights;
    }

    std::string select(const std::vector<std::string>& instance) override;

private:
    std::atomic<int> current_index_;
    std::unordered_map<std::string, int> weights_;
};

} //namespace mooncrpc