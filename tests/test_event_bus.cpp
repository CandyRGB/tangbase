#include "event_bus.hpp"
#include <iostream>
#include <string>
#include <atomic>

using namespace tangbase;

struct UserEvent {
    std::string name;
    int age;
};

struct PingEvent {
    int sequence;
};

int main() {
    EventBus bus;

    // 测试基本订阅/发布
    {
        std::atomic<int> counter{0};
        std::string last_name;

        auto id = bus.subscribe<UserEvent>([&](const UserEvent& e) {
            last_name = e.name;
            counter++;
        });

        bus.publish(UserEvent{"Alice", 30});
        bus.publish(UserEvent{"Bob", 25});

        bus.unsubscribe<UserEvent>(id);

        bus.publish(UserEvent{"Charlie", 35});

        if (counter.load() == 2 && last_name == "Bob") {
            std::cout << "[PASS] basic subscribe/publish test" << std::endl;
        } else {
            std::cerr << "[FAIL] basic test: counter=" << counter.load() << ", last_name=" << last_name << std::endl;
            return 1;
        }
    }

    // 测试不同事件类型
    {
        std::atomic<int> ping_count{0};
        std::atomic<int> user_count{0};

        bus.subscribe<PingEvent>([&](const PingEvent& e) {
            ping_count++;
        });

        bus.subscribe<UserEvent>([&](const UserEvent& e) {
            user_count++;
        });

        bus.publish(PingEvent{1});
        bus.publish(PingEvent{2});
        bus.publish(UserEvent{"Dave", 40});

        if (ping_count.load() == 2 && user_count.load() == 1) {
            std::cout << "[PASS] multiple event types test" << std::endl;
        } else {
            std::cerr << "[FAIL] multiple event types test" << std::endl;
            return 1;
        }
    }

    // 测试 clear
    {
        std::atomic<int> counter{0};
        bus.subscribe<PingEvent>([&](const PingEvent&) { counter++; });

        bus.publish(PingEvent{1});
        bus.clear();
        bus.publish(PingEvent{2});

        if (counter.load() == 1) {
            std::cout << "[PASS] clear test" << std::endl;
        } else {
            std::cerr << "[FAIL] clear test: counter=" << counter.load() << std::endl;
            return 1;
        }
    }

    std::cout << "\n=== All tests passed ===" << std::endl;
    return 0;
}