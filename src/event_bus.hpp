#pragma once
// tangbase::EventBus — 类型安全的事件总线，支持发布/订阅模式
// 依赖 C++17, <functional>, <mutex>, <unordered_map>, <vector>, <typeindex>

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>

namespace tangbase {

class EventBus {
public:
    EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    template<class T>
    int subscribe(std::function<void(const T&)> handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto id = next_id_++;
        get_subscribers<T>().push_back({id, std::move(handler)});
        return id;
    }

    template<class T>
    void unsubscribe(int id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& subs = get_subscribers<T>();
        subs.erase(
            std::remove_if(subs.begin(), subs.end(),
                [id](const auto& s) { return s.id == id; }),
            subs.end()
        );
    }

    template<class T>
    void publish(const T& event) {
        std::vector<std::function<void(const T&)>> callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& sub : get_subscribers<T>()) {
                callbacks.push_back(sub.handler);
            }
        }
        for (auto& cb : callbacks) {
            cb(event);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.clear();
    }

private:
    template<class T>
    struct Entry {
        int id;
        std::function<void(const T&)> handler;
    };

    template<class T>
    struct VectorHolder {
        std::vector<Entry<T>> entries;
    };

    template<class T>
    std::vector<Entry<T>>& get_subscribers() {
        auto key = std::type_index(typeid(T));
        auto it = callbacks_.find(key);
        if (it == callbacks_.end()) {
            auto holder = std::make_shared<VectorHolder<T>>();
            callbacks_[key] = holder;
            return holder->entries;
        }
        return std::static_pointer_cast<VectorHolder<T>>(it->second)->entries;
    }

    std::unordered_map<std::type_index, std::shared_ptr<void>> callbacks_;
    int next_id_ = 0;
    mutable std::mutex mutex_;
};

}  // namespace tangbase