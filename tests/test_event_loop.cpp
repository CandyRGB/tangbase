#include "event_loop.hpp"
#include "thread_pool.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

using namespace tangbase;

int main() {
    auto main_id = std::this_thread::get_id();

    // 测试关联线程池
    {
        auto pool = std::make_shared<ThreadPool>(2);
        EventLoop loop(pool);

        std::atomic<int> counter{0};
        std::thread::id callback_id;

        auto t1 = loop.add_timer(50, [&] {
            callback_id = std::this_thread::get_id();
            counter++;
        });
        auto t2 = loop.add_timer(100, [&] {
            counter++;
            loop.stop();
        });

        loop.run();

        // 验证任务不在主线程（事件循环线程）执行，而是在线程池线程
        if (callback_id != main_id && counter.load() == 2) {
            std::cout << "[PASS] tasks run on thread pool, not event loop thread" << std::endl;
        } else {
            std::cerr << "[FAIL] expected task to run on thread pool" << std::endl;
            return 1;
        }
    }

    // 测试无线程池时在主线程（事件循环线程）执行
    {
        EventLoop loop(nullptr);

        std::atomic<int> counter{0};
        std::thread::id callback_id;

        auto t = loop.add_timer(50, [&] {
            callback_id = std::this_thread::get_id();
            counter++;
            loop.stop();
        });

        loop.run();

        if (callback_id == main_id && counter.load() == 1) {
            std::cout << "[PASS] without thread pool, tasks run on event loop thread" << std::endl;
        } else {
            std::cerr << "[FAIL] without thread pool" << std::endl;
            return 1;
        }
    }

    std::cout << "\n=== All tests passed ===" << std::endl;
    return 0;
}