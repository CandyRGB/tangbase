#include <thread_pool.hpp>
#include <result.hpp>
#include <logger.hpp>
#include <cassert>
#include <iostream>

int main() {
    // === Result test ===
    {
        auto r = tangbase::Result<int, std::string>::ok(42);
        assert(r.is_ok());
        assert(*r.ok() == 42);

        auto r2 = tangbase::Result<int, std::string>::err("oops");
        assert(r2.is_err());
        assert(*r2.err() == "oops");

        auto r3 = r.map([](int v) { return v * 2; });
        assert(r3.is_ok());
        assert(*r3.ok() == 84);

        auto r4 = r2.map_err([](const std::string& e) { return e.size(); });
        assert(r4.is_err());
        assert(*r4.err() == 4);

        auto r5 = r.map([](int v) { return v + 1; });
        assert(r5.is_ok());
        assert(r5.ok() != nullptr);
        int r5val = *r5.ok();
        assert(r5val == 43);

        assert(r.unwrap_or(0) == 42);
    }

    // === Option test ===
    {
        tangbase::Option<int> some{42};
        tangbase::Option<int> none;
        assert(some.has_value());
        assert(*some == 42);
        assert(!none.has_value());
        assert(some.value_or(99) == 42);
        assert(none.value_or(99) == 99);
    }

    // === Logger test ===
    {
        tangbase::Logger::outconsole();
        tangbase::log_debug("debug msg");
        tangbase::log_info("info msg");
        tangbase::log_warn("warn msg");
        tangbase::log_error("error msg");

        tangbase::Logger::outfile("test.log");
        tangbase::log_info("file log msg");
    }

    // === ThreadPool test ===
    {
        tangbase::ThreadPool pool(2);
        auto f = pool.enqueue([] { return 123; });
        assert(f.has_value());
        assert(f->get() == 123);

        // Test shutdown
        pool.shutdown();
        auto f2 = pool.enqueue([] { return 456; });
        assert(!f2.has_value());

        pool.join();
    }

    std::cout << "All tests passed.\n";
    return 0;
}