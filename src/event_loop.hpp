#pragma once
// tangbase::EventLoop — 单线程事件循环，跨平台实现
// 依赖 C++17, <vector>, <queue>, <chrono>, <functional>, <atomic>, <condition_variable>, <thread>

#include <chrono>
#include <functional>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <cassert>

#include "thread_pool.hpp"

namespace tangbase {

class Handle {
public:
    Handle() = default;
    virtual ~Handle() = default;

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept : closed_(other.closed_.load()) {
        other.closed_.store(true);
    }

    void close() {
        closed_.store(true);
        on_close();
    }

    bool is_closed() const { return closed_.load(); }

protected:
    virtual void on_close() {}

private:
    std::atomic<bool> closed_{false};
};

class TimerHandle : public Handle {
public:
    using Callback = std::function<void()>;

    TimerHandle(std::chrono::steady_clock::time_point expire, int64_t repeat_ms, Callback cb)
        : expire_(expire), repeat_(repeat_ms), callback_(std::move(cb)) {}

    std::chrono::steady_clock::time_point expire() const { return expire_; }
    int64_t repeat() const { return repeat_; }
    Callback& callback() { return callback_; }
    bool& fired() { return fired_; }

protected:
    void on_close() override { callback_ = nullptr; }

private:
    std::chrono::steady_clock::time_point expire_;
    int64_t repeat_{0};
    Callback callback_;
    bool fired_{false};
};

class IdleHandle : public Handle {
public:
    using Callback = std::function<void()>;

    explicit IdleHandle(Callback cb) : callback_(std::move(cb)) {}

    bool& fired() { return fired_; }
    Callback& callback() { return callback_; }

protected:
    void on_close() override { callback_ = nullptr; }

private:
    Callback callback_;
    bool fired_{false};
};

class AsyncHandle : public Handle {
public:
    using Callback = std::function<void()>;

    explicit AsyncHandle(Callback cb) : callback_(std::move(cb)) {}

    void send() {
        if (!is_closed()) {
            signaled_.store(true);
            notify();
        }
    }

    bool consume() {
        if (!is_closed() && signaled_.exchange(false)) {
            if (callback_) callback_();
            return true;
        }
        return false;
    }

protected:
    virtual void notify() = 0;
    void on_close() override { callback_ = nullptr; }

private:
    std::atomic<bool> signaled_{false};
    Callback callback_;
};

class EventLoop {
public:
    explicit EventLoop(std::shared_ptr<ThreadPool> pool = nullptr);

    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    std::shared_ptr<TimerHandle> add_timer(int64_t delay_ms, TimerHandle::Callback callback) {
        auto expire = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
        return add_timer_impl(expire, 0, std::move(callback));
    }

    std::shared_ptr<TimerHandle> add_repeat_timer(int64_t delay_ms, int64_t repeat_ms, TimerHandle::Callback callback) {
        auto expire = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
        return add_timer_impl(expire, repeat_ms, std::move(callback));
    }

    std::shared_ptr<IdleHandle> add_idle(IdleHandle::Callback callback);

    std::shared_ptr<AsyncHandle> add_async(AsyncHandle::Callback callback);

    bool run(int mode = 0);
    void stop();

    bool is_running() const { return running_.load(); }

    std::shared_ptr<ThreadPool> thread_pool() const { return pool_; }

private:
    friend class CondVarAsyncHandle;

    std::shared_ptr<TimerHandle> add_timer_impl(
        std::chrono::steady_clock::time_point expire, int64_t repeat_ms, TimerHandle::Callback callback);

    void update_time();

    std::shared_ptr<ThreadPool> pool_;
    std::vector<std::shared_ptr<TimerHandle>> timers_;
    std::vector<std::shared_ptr<IdleHandle>> idles_;
    std::vector<std::shared_ptr<AsyncHandle>> asyncs_;
    std::vector<std::shared_ptr<TimerHandle>> repeating_timers_;

    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    std::chrono::steady_clock::time_point now_;
};

class CondVarAsyncHandle : public AsyncHandle {
public:
    explicit CondVarAsyncHandle(std::condition_variable& cond, Callback cb);

    void notify() override;

protected:
    void on_close() override {}

private:
    std::condition_variable& cond_;
};

inline EventLoop::EventLoop(std::shared_ptr<ThreadPool> pool)
    : pool_(std::move(pool)), now_(std::chrono::steady_clock::now()) {
}

inline EventLoop::~EventLoop() {
    stop();
}

inline void EventLoop::stop() {
    stopped_.store(true);
    cond_.notify_all();
}

inline void EventLoop::update_time() {
    now_ = std::chrono::steady_clock::now();
}

inline std::shared_ptr<TimerHandle> EventLoop::add_timer_impl(
    std::chrono::steady_clock::time_point expire, int64_t repeat_ms, TimerHandle::Callback callback) {
    auto handle = std::make_shared<TimerHandle>(expire, repeat_ms, std::move(callback));
    std::lock_guard<std::mutex> lock(mutex_);
    timers_.push_back(handle);
    return handle;
}

inline std::shared_ptr<IdleHandle> EventLoop::add_idle(IdleHandle::Callback callback) {
    auto handle = std::make_shared<IdleHandle>(std::move(callback));
    std::lock_guard<std::mutex> lock(mutex_);
    idles_.push_back(handle);
    return handle;
}

inline std::shared_ptr<AsyncHandle> EventLoop::add_async(AsyncHandle::Callback callback) {
    auto handle = std::make_shared<CondVarAsyncHandle>(cond_, std::move(callback));
    std::lock_guard<std::mutex> lock(mutex_);
    asyncs_.push_back(handle);
    return handle;
}

inline CondVarAsyncHandle::CondVarAsyncHandle(std::condition_variable& cond, Callback cb)
    : AsyncHandle(std::move(cb)), cond_(cond) {
}

inline void CondVarAsyncHandle::notify() {
    cond_.notify_one();
}

inline bool EventLoop::run(int mode) {
    if (running_.exchange(true)) return false;
    stopped_.store(false);
    update_time();

    while (!stopped_.load()) {
        update_time();

        std::vector<std::shared_ptr<TimerHandle>> to_fire;
        std::vector<std::shared_ptr<TimerHandle>> to_repeat;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            for (auto& timer : timers_) {
                if (timer->is_closed()) continue;
                if (!timer->fired() && now_ >= timer->expire()) {
                    timer->fired() = true;
                    to_fire.push_back(timer);
                    if (timer->repeat() > 0) {
                        auto next_expire = now_ + std::chrono::milliseconds(timer->repeat());
                        auto rt = std::make_shared<TimerHandle>(next_expire, timer->repeat(), timer->callback());
                        to_repeat.push_back(rt);
                    }
                }
            }

            for (auto& timer : repeating_timers_) {
                if (timer->is_closed()) continue;
                if (now_ >= timer->expire()) {
                    to_fire.push_back(timer);
                    if (timer->repeat() > 0) {
                        auto next_expire = now_ + std::chrono::milliseconds(timer->repeat());
                        auto rt = std::make_shared<TimerHandle>(next_expire, timer->repeat(), timer->callback());
                        to_repeat.push_back(rt);
                    }
                }
            }

            auto timer_end = std::remove_if(timers_.begin(), timers_.end(),
                [](const auto& h) { return h->is_closed(); });
            timers_.erase(timer_end, timers_.end());

            auto repeat_end = std::remove_if(repeating_timers_.begin(), repeating_timers_.end(),
                [](const auto& h) { return h->is_closed(); });
            repeating_timers_.erase(repeat_end, repeating_timers_.end());

            for (auto& rt : to_repeat) {
                repeating_timers_.push_back(rt);
            }
        }

        for (auto& timer : to_fire) {
            if (!timer->is_closed() && timer->callback()) {
                if (pool_) {
                    pool_->enqueue(timer->callback());
                } else {
                    timer->callback()();
                }
            }
        }

        bool has_async = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& as : asyncs_) {
                if (as->is_closed()) continue;
                has_async = true;
            }
        }

        std::vector<std::shared_ptr<IdleHandle>> idles_to_fire;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& idle : idles_) {
                if (idle->is_closed() || idle->fired()) continue;
                idle->fired() = true;
                idles_to_fire.push_back(idle);
            }
        }

        for (auto& idle : idles_to_fire) {
            if (!idle->is_closed() && idle->callback()) {
                idle->callback()();
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto idle_end = std::remove_if(idles_.begin(), idles_.end(),
                [](const auto& h) { return h->is_closed(); });
            idles_.erase(idle_end, idles_.end());
        }

        if (mode == 1) {
            running_.store(false);
            return !to_fire.empty() || !idles_to_fire.empty();
        }

        if (timers_.empty() && repeating_timers_.empty() && !has_async && idles_.empty()) {
            running_.store(false);
            return false;
        }

        int64_t sleep_ms = 100;
        bool has_pending = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!timers_.empty() || !repeating_timers_.empty()) {
                auto next_expire = (std::chrono::steady_clock::time_point::max)();
                for (auto& t : timers_) {
                    if (!t->is_closed() && !t->fired() && t->expire() < next_expire) {
                        next_expire = t->expire();
                    }
                }
                for (auto& t : repeating_timers_) {
                    if (!t->is_closed() && t->expire() < next_expire) {
                        next_expire = t->expire();
                    }
                }
                if (next_expire != (std::chrono::steady_clock::time_point::max)()) {
                    auto diff = next_expire - now_;
                    if (diff < (std::chrono::steady_clock::duration::zero)()) {
                        sleep_ms = 0;
                        has_pending = true;
                    } else {
                        sleep_ms = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(diff).count());
                    }
                }
            }
        }

        if (has_pending || sleep_ms == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(0));
        } else {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait_for(lock, std::chrono::milliseconds(sleep_ms), [this] {
                return stopped_.load();
            });
        }
    }

    running_.store(false);
    return false;
}

}  // namespace tangbase