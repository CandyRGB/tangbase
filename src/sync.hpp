#pragma once
// tangbase::Latch   — one-time barrier; wait() blocks until count reaches 0
// tangbase::BinarySemaphore — two-state (available/unavailable); acquire/release
// tangbase::CountingSemaphore — N-count semaphore; used for resource limiting

#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <mutex>

namespace tangbase {

// Latch — a single-use barrier.
// All threads must call arrive() the required number of times before wait() unblocks.
// Once opened (count reaches 0), Latch remains in the "done" state forever.
// NOT thread-safe for concurrent copy/assignment.
class Latch {
public:
    // Create a latch with the given expected arrival count.
    // Throws std::invalid_argument if expected < 0.
    explicit Latch(std::ptrdiff_t expected) : count_(expected) {
        if (expected < 0) throw std::invalid_argument("Latch: expected < 0");
    }

    Latch(const Latch&) = delete;
    Latch& operator=(const Latch&) = delete;

    // Decrement the counter by n. When count <= 0, all waiting threads unblock.
    // The latch is permanently opened after this call reaches zero.
    void arrive(std::ptrdiff_t n = 1) {
        auto prev = count_.fetch_sub(n, std::memory_order_acq_rel);
        if (prev - n <= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            done_ = true;
            cv_.notify_all();
        }
    }

    // Block until the latch is opened (count <= 0). Spurious wakeups are possible;
    // always re-check the condition in a loop or use the predicate form.
    void wait() const {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return done_; });
    }

private:
    std::atomic<std::ptrdiff_t> count_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    bool done_ = false;
};

// BinarySemaphore — a two-state (available/unavailable) semaphore.
// Use when a single resource must be shared across threads with explicit acquire/release.
// NOT thread-safe for concurrent copy/assignment.
class BinarySemaphore {
public:
    // Create a binary semaphore.
    // initial = true  → starts in the "available" state (acquire succeeds without blocking)
    // initial = false → starts in the "unavailable" state (first acquire blocks)
    explicit BinarySemaphore(bool initial = true) : available_(initial) {}

    BinarySemaphore(const BinarySemaphore&) = delete;
    BinarySemaphore& operator=(const BinarySemaphore&) = delete;

    // Acquire (decrement). Blocks until the semaphore is available.
    void acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return available_.load(std::memory_order_acquire); });
        available_.store(false, std::memory_order_release);
    }

    // Try to acquire without blocking. Returns true on success, false if unavailable.
    bool try_acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!available_.load(std::memory_order_acquire)) return false;
        available_.store(false, std::memory_order_release);
        return true;
    }

    // Release (increment). Unblocks one waiting thread, if any.
    void release() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            available_ = true;
        }
        cv_.notify_one();
    }

private:
    std::atomic_bool available_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
};

// CountingSemaphore — a semaphore with a non-negative integer count.
// Used to limit concurrent access to a pool of N identical resources.
// NOT thread-safe for concurrent copy/assignment.
class CountingSemaphore {
public:
    // Create a counting semaphore with max count max and initial count initial.
    // initial must be in [0, max].
    // Throws std::invalid_argument if initial is out of range.
    explicit CountingSemaphore(std::ptrdiff_t max, std::ptrdiff_t initial = 0)
        : max_(max), count_(initial) {
        if (initial < 0 || initial > max) throw std::invalid_argument("CountingSemaphore: bad initial");
    }

    CountingSemaphore(const CountingSemaphore&) = delete;
    CountingSemaphore& operator=(const CountingSemaphore&) = delete;

    // Acquire (decrement). Blocks if count == 0.
    void acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return count_ > 0; });
        --count_;
    }

    // Try to acquire without blocking. Returns true on success, false if count == 0.
    bool try_acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ <= 0) return false;
        --count_;
        return true;
    }

    // Release (increment). Unblocks one waiting thread, if any. Does not exceed max.
    void release() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (count_ < max_) ++count_;
        }
        cv_.notify_one();
    }

    // Returns the maximum count (the size of the resource pool).
    std::ptrdiff_t max() const { return max_; }

private:
    std::ptrdiff_t max_;
    std::atomic<std::ptrdiff_t> count_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
};

}  // namespace tangbase