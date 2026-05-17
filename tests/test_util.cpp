#include <util/file.hpp>
#include <util/string.hpp>
#include <datetime.hpp>
#include <cassert>
#include <cstdio>

int main() {
    // --- string util ---
    using namespace tangbase::util;

    assert(trim("  hello  ") == "hello");
    assert(trim("hello\n") == "hello");
    assert(!trim("hello").empty());
    assert(starts_with("hello world", "hello"));
    assert(!starts_with("hello world", "world"));
    assert(ends_with("hello world", "world"));
    assert(ends_with("file.txt", ".txt"));

    auto parts = split("a,b,c", ',');
    assert(parts.size() == 3 && parts[0] == "a" && parts[2] == "c");

    assert(replace_once("aaa", "aa", "b") == "ba");
    assert(replace_all("aaa", "a", "b") == "bbb");

    assert(to_lower("Hello") == "hello");
    assert(to_upper("Hello") == "HELLO");

    // --- datetime ---
    auto now = tangbase::DateTime::now();
    assert(!now.to_iso8601().empty());
    assert(!now.to_datetime().empty());
    assert(!now.to_date().empty());
    assert(!now.to_time().empty());
    assert(now.ago() == "0s ago");

    std::printf("String and DateTime tests passed.\n");
    return 0;
}