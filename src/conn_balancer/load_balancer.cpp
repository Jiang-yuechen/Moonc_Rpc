#include "load_balancer.h"
#include "log/log_manager.h"

namespace mooncrpc {

// 静态成员类外初始化
std::mutex LoadBalancer::mutex_;
std::shared_ptr<LoadBalancer> LoadBalancer::instance_ = nullptr;

// 负载均衡器工厂(内部结构体，隐藏实现细节)
struct BalancerFactory {
    static std::shared_ptr<LoadBalancer> createRandom() {
        return std::make_shared<RandomLoadBalancer>();
    }

    static std::shared_ptr<LoadBalancer> createRoundRobin() {
        return std::make_shared<RoundRobinLoadBalancer>();
    }

    static std::shared_ptr<LoadBalancer> createWeighted() {
        return std::make_shared<WeightedRoundRobinLoadBalancer>();
    }

    // 获取工厂映射表
    static const std::unordered_map<std::string, std::function<std::shared_ptr<LoadBalancer>()>>& getMap() {
        static const std::unordered_map<std::string, std::function<std::shared_ptr<LoadBalancer>()>> map = {
            {"random", createRandom},
            {"round", createRoundRobin},
            {"weight", createWeighted}
        };
        return map;
    }
};

// 单例与核心暴露接口
std::shared_ptr<LoadBalancer> LoadBalancer::getInstance() {
    if (!instance_) {
        // 实例不存在
        std::lock_guard<std::mutex> lock(mutex_);
        if (!instance_) {
            instance_ = BalancerFactory::createRandom();    // 默认使用随机
        }
    }
    return instance_;
}

/**
 * @brief 根据配置类型初始化全局负载均衡器
 * @param type 负载均衡策略类型 ("random", "round", "weight")
 * @return true 初始化成功
 * @return false 初始化失败，已回退到默认随机策略
 */
bool LoadBalancer::initBalancer(const std::string& type) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& balancer_map = BalancerFactory::getMap();
    auto it = balancer_map.find(type);

    // 查找到对应的策略工厂方法
    if (it != balancer_map.end()) {
        try {
            instance_ = it->second();
            CLIENT_INFO("Initialized [{}] load balancer", type);
            return true;
        } catch (const std::exception& e) {
            CLIENT_ERROR("Failed to initialize [{}] load balancer: {}", type, e.what());
            // 发生异常时进行降级兜底
            instance_ = BalancerFactory::createRandom();
            return false;
        }
    }

    // 传入了一个不支持的类型，降级为默认随机策略
    CLIENT_WARN("Unknown load balancer type: [{}], falling back to random", type);
    instance_ = BalancerFactory::createRandom();
    return false;
}

/**
 * @brief 暴露给调用方的统一路由接口
 * @param servers 当前存活的服务端节点列表 (IP:Port)
 * @return std::string 被选中的服务端节点
 */
std::string LoadBalancer::selectServer(const std::vector<std::string>& servers) {
    // 异常拦截：如果上层未初始化，这里自动补救
    if (!instance_) {
        CLIENT_WARN("Load balancer not initialized, using default random balancer");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                instance_ = std::make_shared<RandomLoadBalancer>();
            }
        }
    }

    // 异常拦截：节点列表为空
    if (servers.empty()) {
        CLIENT_ERROR("No instances available for load balancing");
        return "";
    }

    try {
        // 多态调用具体子类的负载均衡策略
        return instance_->select(servers);
    } catch (const std::exception& e) {
        CLIENT_ERROR("Error in load balancer select: {}", e.what());
        return servers[0];      // 极端情况下最终降级：直接返回第一个节点
    }
}

/**
 * @brief 随机负载均衡器实现
 * @param instances 服务端节点列表
 * @return std::string 被选中的节点
 * @details 利用 thread_local 隔离随机数引擎，彻底消除高并发下的锁竞争瓶颈。
 */
std::string RandomLoadBalancer::select(const std::vector<std::string>& instances) {
    // thread_local 保证每个调用线程都有自己独立的一套随机数生成器
    // 如果使用全局静态变量，多线程并发生成随机数时必须加锁，严重拖慢性能
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());

    // 生成 [0 ~ size-1] 之间的均匀分布整数
    std::uniform_int_distribution<> dis(0, instances.size() - 1);
    return instances[dis(gen)];
}

/**
 * @brief 轮询负载均衡器实现
 * @param instances 服务端节点列表
 * @return std::string 被选中的节点
 * @details 基于 std::atomic 的 fetch_add 无锁技术实现极速轮询。
 */
std::string RoundRobinLoadBalancer::select(const std::vector<std::string>& instances) {
    // fetch_add 是原子操作，先获取旧值再 add
    // 1：增量
    // memory_order_relaxed 表示宽松内存序，不要求强烈的内存屏障，提高性能
    size_t current_index = current_index_.fetch_add(1, std::memory_order_relaxed);

    // 取模让指针循环回到头部
    return instances[current_index % instances.size()];
}

/**
 * @brief 加权轮询负载均衡器实现
 * @param instances 服务端节点列表
 * @return std::string 被选中的节点
 * @details 采用计算总权重后生成原子递增游标的方法，映射到对应的权重区间中。
 */
std::string WeightedRoundRobinLoadBalancer::select(const std::vector<std::string>& instances) {
    // 1. 构建本次请求的实际权重数组（默认权重为1）
    std::vector<int> weights(instances.size(), 1);

    for (size_t i = 0; i < instances.size(); i++) {
        auto it = weights_.find(instances[i]);
        if (it != weights_.end()) {
            weights[i] = it->second;
        }
    }

    // 2. 计算当前所有存活节点的总权重值
    int total_weight = 0;
    for (int weight : weights) {
        total_weight += weight;
    }

    // 如果所有权重都配置为 0，直接返回首节点
    if (total_weight == 0) {
        return instances[0];
    }

    // 3. 底层全局游标并对总权重取模，得到本次的命中目标值
    // 这里使用无锁原子操作规避并发覆盖 bug
    int target = current_index_.fetch_add(1, std::memory_order_relaxed) % total_weight;

    // 4. 区间扫描：累加当前扫描过的权重，当累计值超过 target 时，即命中该区间对应的节点
    // 例如：权重为 [5, 2, 3]，总权重10。target 若为 6，
    // 第一轮 0+5=5，6<5 不成立。第二轮 5+2=7，6<7 成立，命中节点 B
    int current_weight = 0;
    for (size_t i = 0; i < instances.size(); i++) {
        current_weight += weights[i];
        if (target < current_weight) {
            return instances[i];
        }
    }

    // 5. 极端边界兜底
    return instances[0];
}

} // namespace mooncrpc