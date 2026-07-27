#include "core/rpc_client.h"
#include "message.pb.h"
#include "log/log_manager.h"
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <string>
#include <iomanip> // 用于格式化输出

using namespace mooncrpc;

void BenchmarkConcurrentCalls() {
    // 配置压测参数：10个线程，每个线程并发发起 10000 次请求，总计 10 万次
    const int num_threads = 10;
    const int calls_per_thread = 10000;
    
    std::atomic<int> total_success{0};
    std::atomic<int> total_failed{0};

    // 构造 1KB 大小的负载数据 (1024 字节)
    std::string payload_1kb(1024, 'A');

    std::cout << "Starting MooncRPC Benchmark..." << std::endl;
    std::cout << "Payload Size: 1 KB | Total Requests: 100,000 | Threads: 10" << std::endl;
    
    // 记录压测开始时间
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, calls_per_thread, &payload_1kb, &total_success, &total_failed]() {
            // 每个线程独立初始化一个 RpcClient (各自维护与 ZK 和 Server 的连接)
            // 依赖你的 rpc_client.json 配置文件
            RpcClient client("../config/rpc_client.json");

            for (int i = 0; i < calls_per_thread; ++i) {
                // 1. 组装 Protobuf 请求
                LoginRequest request;
                // 将 1KB 的负载塞入 username 字段中模拟大包传输
                request.set_username(payload_1kb); 
                request.set_password("benchmark_test");

                LoginResponse response;
                
                // 2. 发起同步 RPC 调用
                // 结合你的 RpcClient::Call 接口：需要传入服务名、Protobuf Request、Protobuf Response
                bool rpc_status = client.Call("UserService", request, response);

                // 3. 统计结果 
                // 只要 rpc_status 为 true，就说明整个网络流转和框架层是成功的！
                // 我们不需要校验 response.seccess() 的业务登录结果。
                if (rpc_status) {
                    total_success++;
                } else {
                    total_failed++;
                }
            }
        });
    }

    // 等待所有线程执行完毕
    for (auto& t : threads) {
        t.join();
    }

    // 记录压测结束时间
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 性能指标计算
    int total_calls = total_success + total_failed;
    double qps = (total_calls * 1000.0) / duration.count();
    double failure_rate = (total_failed * 100.0) / total_calls;

    // 输出压测报告
    std::cout << "\n==========================================" << std::endl;
    std::cout << "         MooncRPC Benchmark Results       " << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << " Total calls:  " << total_calls << std::endl;
    std::cout << " Success:      " << total_success << std::endl;
    std::cout << " Failed:       " << total_failed << std::endl;
    std::cout << " Failure Rate: " << std::fixed << std::setprecision(2) << failure_rate << " %" << std::endl;
    std::cout << " Time Cost:    " << duration.count() << " ms" << std::endl;
    std::cout << " QPS:          " << std::fixed << std::setprecision(2) << qps << " req/sec" << std::endl;
    std::cout << "==========================================" << std::endl;
}

int main() {
    // 1. 初始化日志系统
    if (!Logger::GetInstance().Init("../config/log_config.json")) {
        std::cerr << "Failed to init logger." << std::endl;
        return -1;
    }

    // 【极其关键的压测优化】：必须动态调整日志级别！
    // 你的项目中提到了：“在 RPC 网络层里，不能每次 read/write 都打大量 INFO 日志，否则 QPS 上去后日志本身会成为瓶颈”
    // 因此在 10 万次极速并发压测期间，我们将日志级别强制设置为 ERROR，屏蔽海量的 DEBUG 和 INFO 输出
    spdlog::set_level(spdlog::level::err); 

    // 2. 启动压测
    BenchmarkConcurrentCalls();

    return 0;
}