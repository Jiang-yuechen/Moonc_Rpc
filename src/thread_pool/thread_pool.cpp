#include "thread_pool.h"

namespace mooncrpc {

//基于配置的构造函数
ThreadPool::ThreadPool(const ThreadPoolConfig& config) :config_(config) {
    workers_.reserve(config.max_threads);   //预分配最大线程数
    thread_last_active_.resize(config_.max_threads);    //可以记录最大线程数的存活时间

    //线程池启动时只创建核心线程数的工人
    for (size_t i = 0; i < config_.core_threads; i++) {
        workers_.emplace_back(&ThreadPool::WorkerThread, this);
        thread_last_active_[i] = std::chrono::steady_clock::now();
    }
    current_threads_ = config_.core_threads;    //更新活跃线程数
    LOG_INFO("[ThreadPool] Initialized! CoreThreads: {}, Max_threads: {}", config_.core_threads, config_.max_threads);
}

//析构函数
ThreadPool::~ThreadPool() {
    //线程池还未关闭则关闭线程池
    if (state_ != ThreadPoolState::STOPPED) {
        StopNow();
    }
}

//工作线程主循环：消费者逻辑
void ThreadPool::WorkerThread() {
    while (true) {
        TaskWrapper task_wrapper{};     //任务包装器结构体

        {
            //1.获取互斥锁，准备去任务队列拿去任务
            std::unique_lock<std::mutex> lock(queue_mutex_);

            /**
             * @brief 2.线程等待休眠：使用条件变量wait_for,只有当新任务到来，或者线程池停止时才会被唤醒
             * @param lock 传入的是std::unique_lock，当线程执行wait_for休眠时底层会自动把queue_mutex_解锁
             * 被唤醒或者超时醒来后会重新给queue_mutex_上锁然后继续执行后面代码 
             * @param keep_alive_time 非核心线程最长休眠时间，超时就会把线程强制唤醒
             * @param lambda OS 有时会因为底层的中断信号意外把休眠的线程唤醒，所以系统规定：
             * 每次醒来都先执行这个Lambda检查一遍，return ture才真正起床，否则继续休眠。 */
            bool has_task = condition_.wait_for(lock, config_.keep_alive_time, [this] {
                return stop_ || !tasks_.empty();
            });

            //3.超时回收机制
            //如果没抢到任务，且当前线程数超过了核心线程数，说明自己是临时工可以被裁
            if (!has_task && current_threads_ > config_.core_threads) {
                current_threads_--;
                LOG_DEBUG("[ThreadPool] A non-core thread was automatically reclaimed due to an idle timeout.");
                return;     //线程函数return 意味着这个线程真正消亡了
            }

            //4.如果线程池关闭并且没有任务，直接下班
            if ((stop_ || state_ == ThreadPoolState::STOPPED) && tasks_.empty()) {
                return;
            }

            //5.抢到任务直接从队列中弹出来
            if (!tasks_.empty()) {
                task_wrapper = std::move(const_cast<TaskWrapper&>(tasks_.top()));   //去除top返回元素的const属性
                tasks_.pop();

                //唤醒可能因队列满而阻塞的提交线程
                if (config_.max_queue_size > 0 && tasks_.size() < config_.max_queue_size) {
                    not_full_condition_.notify_one();
                }
            }
        } //当前线程拿到任务后释放锁

        //6.真正开始执行拿到的用户传进来的任务
        if (task_wrapper.valid()) {
            active_threads_++;  //干活线程+1
            auto start_time = std::chrono::high_resolution_clock::now();

            try {
                task_wrapper.excute();      //执行具体函数代码
                tasks_completed_++;         //完成任务+1
            } catch(const std::exception& e) {
                tasks_failed_++;
                LOG_ERROR("[ThreadPool] Throwing error when excute task: {}.", e.what());
            }

            //记录工作时长
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

            {
                //记录统计时间(加锁)
                std::lock_guard<std::mutex> lock(stats_mutex_);
                total_task_time_ = duration.count() / 1000.0;
            }

            active_threads_--;  //任务结束，活跃线程-1

            //如果正在优雅关闭，并且自己完成了最后一个任务就通知Shutdown彻底关闭
            if (state_ == ThreadPoolState::SHUTTING_DOWN && active_threads_ == 0 && tasks_.empty()) {
                condition_.notify_all();
            }
        }
    }
}

//动态扩容逻辑：任务过多时创建新线程来工作
void ThreadPool::AdjustThreadCount() {
    if (current_threads_ < config_.max_threads && tasks_.size() > active_threads_) {
        try {
            workers_.emplace_back(&ThreadPool::WorkerThread, this);
            current_threads_++;
            LOG_DEBUG("[ThreadPool] Task backlog triggered dynamic scaling, current total number of threads:{}",
                        current_threads.load());
        } catch(...) {
            //防止系统资源不足导致创建新线程失败
            LOG_WARN("[ThreadPool] Failed to create a new thread: system bottleneck reached.");
        }
    }
}

/**
 * @brief 优雅关闭：不再接新任务，并且把任务队列中的任务干完
 * @param wait_timeout_ms 等待超时时间
 * @return 是否优雅关闭 */
bool ThreadPool::Shutdown(std::chrono::milliseconds wait_timeout_ms) {
    ThreadPoolState expect = ThreadPoolState::RUNNING;
    //观察state_是否等于expect：
    //如果等于（匹配成功）：说明此时线程池确实还在正常运行。于是瞬间把 state_ 改为 SHUTTING_DOWN，并返回 true
    //如果不等于（匹配失败）：说明别的线程已经抢先一步改了状态。此时它什么都不改，只是把 expect 变量更新为当前真实的 state_ 值，并返回 false。
    if (!state_.compare_exchange_strong(expect, ThreadPoolState::SHUTTING_DOWN)) {
        return state_ == ThreadPoolState::STOPPED;
    }

    //唤醒所有休眠线程把剩余工作完成
    condition_.notify_all();

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        bool finished = condition_.wait_for(lock, wait_timeout_ms, [this]() {
            return tasks_.empty() && active_threads_ == 0;
        });

        if (!finished) {
            LOG_WARN("[ThreadPool] Graceful shutdown timed out: some tasks remain incomplete.");
            return false;
        }

        //成功关闭：设置为已停止状态
        state_ = ThreadPoolState::STOPPED;
        stop_ = true;
    }
    
    condition_.notify_all();        //通知所有线程退出

    //等待所有线程 join
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    return true;
}

//强制关闭：全部丢弃
void ThreadPool::StopNow() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        state_ = ThreadPoolState::STOPPED;
        stop_ = true;

        //清空任务队列
        while (!tasks_.empty()) {
            tasks_.pop();
        }

        condition_.notify_all();

        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        LOG_INFO("[ThreadPool] Has been destroied.");
    }
}

//Get方法
size_t ThreadPool::GetQueueSize() const noexcept {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

ThreadPoolStats ThreadPool::GetStats() const {
    ThreadPoolStats stats;
    stats.tasks_completed = tasks_completed_.load();
    stats.tasks_failed = tasks_failed_.load();
    stats.active_threads = active_threads_.load();
    stats.queued_tasks = GetQueueSize();
    return stats;
}

//补充一个简易的单参构造函数方便快捷使用
ThreadPool::ThreadPool(size_t threads)
    : ThreadPool(ThreadPoolConfig{threads, threads, 0, std::chrono::milliseconds(60000)}) {};

}