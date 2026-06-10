#pragma once
#include <random>
#include <string>
#include <vector>
#include <random>

namespace mooncrpc {

//负载均衡基类接口
class LoadBalancer {
public:
    virtual ~LoadBalancer() = default;

    //核心接口：挑选可用节点(纯虚函数)
    virtual std::string Select(const std::vector<std::string>& nodes) = 0;
};

//随机负载均衡器实现
class RandonLoadBalancer : public LoadBalancer {
public:
    RandonLoadBalancer() : rd_(), gen_(rd_()) {}
    std::string Select(const std::vector<std::string>& nodes) override {
        if (nodes.empty()) {
            return "";
        }
        // 生成 [0, nodes.size() - 1] 之间的随机索引
        std::uniform_int_distribution<> dis(0, nodes.size() - 1);
        return nodes[dis(gen_)];
    }

private:
    std::random_device rd_;     //真随机数生成器
    std::mt19937 gen_;          //Mersenne Twister 伪随机数生成器
};

} //namespace mooncrpc