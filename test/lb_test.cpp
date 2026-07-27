#include "log/log_manager.h"
#include "conn_balancer/load_balancer.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace mooncrpc;
using json = nlohmann::json;

int main() {
    // 初始化日志系统
    if (!Logger::GetInstance().Init("../config/log_config.json")) {
        return -1;
    }

    CLIENT_INFO("====== Starting Real JSON Load Balancer Factory Test ======");

    // 1. 读取真实的客户端配置文件
    std::string config_file = "../config/rpc_client.json";
    std::ifstream file(config_file);
    if (!file.is_open()) {
        CLIENT_FATAL("Failed to open config file: {}", config_file);
        return -1;
    }

    json config = json::parse(file);
    if (!config.contains("load_balancer") || !config["load_balancer"].contains("weights")) {
        CLIENT_FATAL("Invalid config format: missing load_balancer.weights");
        return -1;
    }

    // 2. 从 JSON 配置中动态提取节点列表和权重表
    std::vector<std::string> nodes;
    std::unordered_map<std::string, int> real_weights;
    
    for (auto& el : config["load_balancer"]["weights"].items()) {
        nodes.push_back(el.key());
        real_weights[el.key()] = el.value().get<int>();
    }

    CLIENT_INFO("Successfully loaded {} nodes from real config.", nodes.size());

    // ---------------------------------------------------------
    // 阶段一：测试随机策略 (Random)
    // ---------------------------------------------------------
    CLIENT_INFO("--- Phase 1: Random Strategy Testing ---");
    LoadBalancer::initBalancer("random");
    
    for (int i = 1; i <= 6; ++i) {
        std::string selected = LoadBalancer::selectServer(nodes);
        CLIENT_INFO("[Random] Request {} routed to: {}", i, selected);
    }

    // ---------------------------------------------------------
    // 阶段二：测试轮询策略 (Round Robin)
    // ---------------------------------------------------------
    CLIENT_INFO("--- Phase 2: Round Robin Strategy Testing ---");
    LoadBalancer::initBalancer("round");
    
    for (int i = 1; i <= 6; ++i) {
        std::string selected = LoadBalancer::selectServer(nodes);
        CLIENT_INFO("[Round Robin] Request {} routed to: {}", i, selected);
    }

    // ---------------------------------------------------------
    // 阶段三：测试加权轮询策略 (Weighted Round Robin)
    // ---------------------------------------------------------
    CLIENT_INFO("--- Phase 3: Weighted Round Robin Testing ---");
    LoadBalancer::initBalancer("weight");
    
    // 注入从真实 JSON 中读取的权重映射表
    LoadBalancer::getInstance()->initWeights(real_weights);

    std::map<std::string, int> hit_count; 
    
    for (int i = 1; i <= 6; ++i) {
        std::string selected = LoadBalancer::selectServer(nodes);
        hit_count[selected]++;
        CLIENT_INFO("[Weighted] Request {} routed to: {}", i, selected);
    }

    // ---------------------------------------------------------
    // 阶段四：加权结果统计校验
    // ---------------------------------------------------------
    CLIENT_INFO("--- Weighted Round Robin Hit Stats (Total 6 requests) ---");
    for (const auto& pair : hit_count) {
        CLIENT_INFO("Node: {}, Actual Hits: {}, Configured Weight: {}", 
                    pair.first, pair.second, real_weights[pair.first]);
    }

    CLIENT_INFO("====== Real JSON Load Balancer Test Completed ======");
    return 0;
}