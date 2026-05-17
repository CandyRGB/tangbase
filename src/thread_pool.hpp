#pragma once
// tangbase::ThreadPool — 线程池实现，用于并行执行任务
// 依赖 C++17, <vector>, <queue>, <thread>, <mutex>, <condition_variable>, <future>, <functional>

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace tangbase {

// 返回默认线程数（CPU 核心数，fallback 为 2）
inline size_t default_thread_count() {
    static size_t n = []{
        size_t c = std::thread::hardware_concurrency();
        return c == 0 ? 2 : c;
    }();
    return n;
}

// ThreadPool — 固定大小的线程池，接受自由函数、lambda、成员函数等任务
// 构造时创建指定数量的工作线程，这些线程持续运行直到 pool 被销毁
// 线程安全：可从多个线程同时 enqueue 任务
class ThreadPool {
public:
    // 构造线程池
    // threads — 工作线程数量，默认使用 CPU 核心数
    // 构造失败（thread 创建失败）会终止已启动的线程并抛出异常
    explicit ThreadPool(size_t threads = default_thread_count())
        : stop_(false) {
        try {
            for (size_t i = 0; i < threads; ++i) {
                workers_.emplace_back([this] { worker_loop(); });
            }
        } catch (...) {
            stop_ = true;
            condition_.notify_all();
            for (auto& t : workers_) {
                if (t.joinable()) t.join();
            }
            throw;
        }
    }

    // 禁止拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 入队一个任务
    // f     — 可调用对象（函数、lambda、bind 表达式等）
    // args  — 传给 f 的参数
    // 返回 std::future<T>（T 为 f 的返回值类型）
    // 若线程池已关闭（shutdown 调用后），返回 std::nullopt
    // 注意：任务捕获的参数被移动到堆上，线程池不持有参数的引用
    template<class F, class... Args>
    std::optional<std::future<std::invoke_result_t<F, Args...>>>
    enqueue(F&& f, Args&&... args) {
        using return_type = std::invoke_result_t<F, Args...>;

        // 将 f 和 args 打包为 packaged_task，保存在共享指针中
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            [func = std::forward<F>(f), tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                return std::apply([&func](auto&&... a) {
                    return std::invoke(func, std::move(a)...);
                }, std::move(tup));
            }
        );

        std::future<return_type> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                return std::nullopt;
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return result;
    }

    // 优雅关闭：不再接受新任务，等待现有任务全部完成
    // 调用后 enqueue 返回 nullopt，已入队的任务会继续执行
    void shutdown() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
    }

    // 等待所有工作线程结束（必须先调用 shutdown）
    // 在 shutdown 后调用，阻塞直到所有工作线程退出
    void join() {
        for (std::thread& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    // 析构：自动调用 shutdown 和 join
    ~ThreadPool() {
        shutdown();
        join();
    }

private:
    // 工作线程主循环：从队列取任务并执行，捕获任务异常防止线程终止
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            try {
                task();
            } catch (...) {
                // 吞掉异常，防止工作线程意外终止
            }
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

}  // namespace tangbase