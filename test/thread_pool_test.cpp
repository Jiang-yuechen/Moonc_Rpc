#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include "../src/thread_pool/thread_pool.h"
#include "../src/log/log_manager.h"

using namespace mooncrpc;

int main() {
    if (!Logger::GetInstance().Init("./test_logs")) {
        return -1;
    }

    LOG_INFO("====== Starting ThreadPool Core Module Test ======");

    // 1. 创建一个只有 2 个核心线程的小型线程池，方便我们观察“队列积压”和“插队”现象
    ThreadPoolParams params;
    params.core_threads = 2;
    params.max_threads = 4;
    params.max_queue_size = 100;
    ThreadPool pool(params);

    LOG_INFO("--- Phase 1: Testing std::future return value ---");
    // 提交一个普通任务，计算 10 + 20
    auto future_result = pool.Enqueue(TaskPriority::NORMAL, [](int a, int b) {
        LOG_INFO("Executing addition task...");
        return a + b;
    }, 10, 20);

    // 阻塞等待并获取结果
    int result = future_result.get();
    LOG_INFO("Successfully got async task result: {}", result);

    LOG_INFO("--- Phase 2: Testing Priority Queue Mechanism ---");
    // 因为我们只有 2 个核心线程，我们先提交 3 个极其耗时的 LOW 优先级任务
    // 这样前 2 个任务会占据所有线程，第 3 个 LOW 任务会被压入队列
    for (int i = 1; i <= 3; ++i) {
        pool.Enqueue(TaskPriority::LOW, [i]() {
            LOG_WARN("[LOW Task {}] Started executing, simulating 2 seconds delay...", i);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            LOG_WARN("[LOW Task {}] Execution completed!", i);
        });
    }

    // 确保上面的 LOW 任务已经塞进去了
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 此时线程池正在全负荷干活，队列里还有 LOW 任务在排队。
    // 我们突然塞入一个 HIGH 优先级任务！
    // 预期结果：一旦有线程空闲出来，它应该无视排在前面的 LOW 任务，优先拿这个 HIGH 任务执行！
    pool.Enqueue(TaskPriority::HIGH, []() {
        LOG_FATAL("🚀 [HIGH Task] I am VVIP, I jumped the queue and executed!");
    });

    // 打印当前监控状态
    auto stats = pool.GetStats();
    LOG_INFO("Current ThreadPool Status -> Active Threads: {}, Queued Tasks: {}, Completed Tasks: {}", 
             stats.active_threads, stats.queued_tasks, stats.tasks_completed);

    LOG_INFO("--- Phase 3: Testing Graceful Shutdown ---");
    LOG_INFO("Main thread initiates Shutdown, waiting for all queued tasks to finish...");
    pool.Shutdown(std::chrono::seconds(10));

    stats = pool.GetStats();
    LOG_INFO("ThreadPool completely stopped. Total completed tasks: {}", stats.tasks_completed);
    
    LOG_INFO("====== ThreadPool Test Completed Successfully ======");
    return 0;
}