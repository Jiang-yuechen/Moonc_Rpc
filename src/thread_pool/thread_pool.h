#pragma once

#include "../src/log/log_manager.h"
#include <memory>
#include <queue>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>

namespace mooncrpc {

// ========================================================
// 1. 基础组件定义 (对应你大纲中的 优先级、状态、配置)
// ========================================================

/**
 * @brief 任务优先级枚举
 */
enum class TaskPriority {
    LOW,            //低优先级任务(eg.后台清理)
    NORMAL,         //普通优先级任务(eg.常规RPC请求)
    HIGH            //高优先级任务(eg.紧急控制指令)
};

/**
 * @brief 线程池生命周期状态枚举类
 */
enum class ThreadPoolState {
    RUNNING,        //正常运行状态：接收新任务
    PAUSED,         //暂停状态：不处理新任务但保持线程存活
    SHUTTING_DOWN,  //关闭中状态：完成队列中的已有任务，但不接受新任务
    STOPPED         //停止状态：所有线程退出
};

/**
 * @brief 线程池动态配置结构体
 */
struct ThreadPoolConfig {
    size_t core_threads = std::thread::hardware_concurrency();    //核心线程数(默认等于CPU核心数)
    size_t max_threads = std::thread::hardware_concurrency() * 2; //最大线程数(按需扩展的上限)
    size_t max_queue_size = 10000;                                //最大队列大小(背压控制，防止内存OOM)
    std::chrono::milliseconds keep_alive_time{60000};        //非核心线程最大空闲时间(默认60s)
};

/**
 * @brief 线程池统计信息结构体(用于监控)
 */
struct ThreadPoolStats {
    size_t tasks_completed = 0;     //已完成任务数
    size_t tasks_failed = 0;        //失败任务数
    size_t active_threads = 0;      //当前正在工作的活跃线程数
    size_t queued_tasks = 0;        //当前正在排队的任务数
};


// ========================================================
// 2. 任务包装器 TaskWrapper
// ========================================================

/**
 * @brief 任务包装器
 * @details C++11 的 std::priority_queue 必须存放同一类型的数据。
 * 但是用户传进来的任务(函数)可能有各种不同的参数和返回值。
 * 这里使用 std::function<void()> 将它们统一进行“类型擦除”来打包。
 * 并带上 priority 参与队列排序。
 */
class TaskWrapper {
public:
    //构造函数
    TaskWrapper(std::function<void()>&& t, TaskPriority p)
        : task_(std::move(t)), priority_(p) {}

    //默认构造
    TaskWrapper() : priority_(TaskPriority::NORMAL) {}

    //执行任务
    void excute() {
        if (task_) {    //检查任务有效性防止执行空函数导致崩溃
            task_();
        }
    }

    //判断当前包装器里有没有真正的任务代码
    bool valid() const { 
        return static_cast<bool>(task_); 
    }

    //重载小于号(<)，用于std::priority_queue的大根堆排序
    //优先级队列默认是大根堆，返回true会把当前元素往后排，大元素(other)往前排。
    //这里priority_ < other.priority_ 保证HIGH(2) 在NORMAL(1)前面
    bool operator<(const TaskWrapper& other) const {
        return static_cast<int>(priority_) < static_cast<int>(other.priority_);
    }

private:
    std::function<void()> task_;        //真正要执行任务(代码块)
    TaskPriority priority_;             //任务的优先级
};

// ========================================================
// 3. 高性能线程池主类
// ========================================================

class ThreadPool {
public:
    //构造函数：固定大小的线程池
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency());

    //构造函数：基于动态配置的线程池
    explicit ThreadPool(const ThreadPoolConfig& config);

    //析构函数
    ~ThreadPool();

    //禁用拷贝与赋值(线程池是非常沉重的系统资源，绝不允许拷贝)
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief 核心函数：向线程池提交任务
     * @details 这个模板函数允许你塞入任意带有任意参数的函数，
     * 并返回一个std::future，让你能在未来某个时刻拿到这个函数的返回值 
     * @tparam F 函数类型
     * @tparam Args 函数参数的类型包
     * @param priority 任务优先级
     * @param f 具体的函数/Lambda
     * @param args 传递给函数的函数
     * @return std::future 包含函数返回值的信
     */
    template<class F, class... Args>
    auto Enqueue(TaskPriority priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>;
    
    //优雅关闭：处理完队列中的剩余任务再关闭
    bool Shutdown(std::chrono::milliseconds wait_timeout_ms = std::chrono::milliseconds::max());

    //强制停止：直接关闭，丢弃所有队列中的任务
    void StopNow();

    //获取监控统计信息
    ThreadPoolStats GetStats() const;
    size_t GetQueueSize() const noexcept;
    ThreadPoolState GetState() const noexcept { return state_.load(); }

private:
    //=== 线程管理部分 ===
    std::vector<std::thread> workers_;                 //线程工作大本营
    std::atomic<size_t> current_threads_{0};        //当前存活的线程数
    std::vector<std::chrono::steady_clock::time_point> thread_last_active_;     //记录每个线程最后工作时间点(用于超时回收)

    //=== 任务队列 ===
    std::priority_queue<TaskWrapper> tasks_;        //优先级任务队列

    //=== 核心并发控制锁与条件变量 ===
    mutable std::mutex queue_mutex_;            //保护任务队列的互斥锁(使用mutable关键字就允许改变锁状态了[加解锁])
    std::condition_variable condition_;         //唤醒工作线程的条件变量(队列有任务 or 要停机了)
    std::condition_variable not_full_condition_;//唤醒提交线程的条件变量(队列不满可以提交任务)
    bool stop_{false};                          //是否收到停机指令

    ThreadPoolConfig config_;                   //线程池配置
    std::atomic<ThreadPoolState> state_{ThreadPoolState::RUNNING};      //线程池状态

    //=== 统计信息 ===
    mutable std::mutex stats_mutex_;            //统计信息互斥锁
    std::atomic<size_t> tasks_completed_{0};    //已完成任务数
    std::atomic<size_t> tasks_failed_{0};       //失败任务数
    std::atomic<size_t> active_threads_{0};     //当前活跃的线程数
    double total_task_time_{0};                   //任务的总执行时间

    //=== 私有函数方法 ===
    void WorkerThread();            //工作线程的主循环(消费者逻辑)
    void AdjustThreadCount();       //动态扩容逻辑
};

// ========================================================
// 4. Enqueue 模板函数的实现 (必须写在头文件里)
// ========================================================
template<typename F, typename... Args>
auto ThreadPool::Enqueue(TaskPriority priority, F&& f, Args&&... args)
    -> std::future<typename std::invoke_result_t<F, Args...>>
{
    //1. C++17: std::invoke_result_t 用于推导函数 f 传入 args 后的返回值类型
    using return_type = typename std::invoke_result_t<F, Args...>;

    //2.将传入的函数和参数绑定起来，并包装成 std::package_task
    //paskage_task 在执行完后会将结果自动写进与它绑定的future中
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    //3.拿到属于这个任务的future, 准备返回给用户
    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        //若线程池已经停止则返回一个空future
        if (stop_) {
            return std::future<return_type>{};
        }

        //4.背压控制机制：如果队列满了，就让提交任务的线程在这里等待
        if (config_.max_queue_size > 0) {
            not_full_condition_.wait(lock, [this] {
                return stop_ || tasks_.size() < config_.max_queue_size;
            });
            if (stop_) {
                return std::future<return_type>{};
            }
        }

        //5.把任务塞进优先级任务队列
        //使用Lambda表达式：外层包装成无参无返回值的void() 函数，内部调用task
        tasks_.emplace(
            [task]() { (*task)(); },
            priority
        );

        //6.动态扩容：若任务比干活的线程数多，且没到最大线程数，就派发一个新线程
        if (tasks_.size() > active_threads_ && current_threads_ < config_.max_threads) {
            AdjustThreadCount();
        }
    }
    //7. 唤醒一个休眠的工作线程工作
    condition_.notify_one();
    return res;

}

} //namespace mooncrpc