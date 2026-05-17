#include "logger.hpp"
#include "thread_pool.hpp"
#include "event_loop.hpp"
#include "event_bus.hpp"
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

using namespace tangbase;

// 事件类型
struct TaskEvent {
    int task_id;
    const char* msg;
};

int main() {
    // 配置日志轮转（每天 + 保留 7 个）
    Logger::RotationOptions opts;
    opts.rotation_interval_ms = 86400000;
    opts.max_files = 7;
    Logger::set_rotation("logs/app_%Y-%m-%d.log", opts);

    log_info("=== Async Task Runner Started ===");

    ThreadPool pool(2);
    EventBus bus;
    EventLoop loop;
    std::atomic<int> completed{0};

    bus.subscribe<TaskEvent>([&](const TaskEvent& e) {
        log_info("task {} completed: {}", e.task_id, e.msg);
        ++completed;
    });

    pool.enqueue([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        bus.publish(TaskEvent{1, "data loaded"});
    });

    pool.enqueue([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        bus.publish(TaskEvent{2, "analysis done"});
    });

    pool.enqueue([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        bus.publish(TaskEvent{3, "report generated"});
    });

    // 添加一个 3 秒超时保护，确保 loop.run() 不会永久阻塞
    loop.add_timer(3000, [&] {
        loop.stop();
    });

    loop.run();

    log_info("=== All tasks completed ({}) ===", completed.load());

    pool.shutdown();
    pool.join();
    return 0;
}