#include <thread_pool.hpp>
#include <cassert>
#include <iostream>

int main() {
    tangbase::ThreadPool pool(2);

    auto f = pool.enqueue([] { return 123; });
    assert(f.has_value());
    assert(f->get() == 123);

    pool.shutdown();
    auto f2 = pool.enqueue([] { return 456; });
    assert(!f2.has_value());

    pool.join();

    std::cout << "ThreadPool tests passed.\n";
    return 0;
}