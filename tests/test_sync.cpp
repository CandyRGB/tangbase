#include <sync.hpp>
#include <thread_pool.hpp>
#include <cassert>
#include <iostream>
#include <thread>

int main() {
    // === Latch test ===
    {
        tangbase::Latch latch(3);

        std::thread t1([&] { latch.arrive(); });
        std::thread t2([&] { latch.arrive(); });
        std::thread t3([&] { latch.arrive(); });

        t1.join(); t2.join(); t3.join();
        latch.wait();  // must not block
    }

    // === BinarySemaphore test ===
    {
        tangbase::BinarySemaphore sem(true);

        sem.acquire();
        bool ok = sem.try_acquire();  // should fail
        assert(!ok);

        sem.release();
        ok = sem.try_acquire();  // should succeed
        assert(ok);
    }

    // === CountingSemaphore test ===
    {
        tangbase::CountingSemaphore sem(3, 3);

        sem.acquire();
        sem.acquire();
        bool ok = sem.try_acquire();  // should succeed (count was 3)
        assert(ok);

        sem.release();
        sem.release();
        sem.release();
        ok = sem.try_acquire();  // should succeed
        assert(ok);
    }

    std::cout << "Sync tests passed.\n";
    return 0;
}