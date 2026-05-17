#include <assert.hpp>
#include <cstdio>

int main() {
    // TB_CHECK is compile-time, just verify it compiles
    TB_CHECK(true);

    // TB_ASSERT passing case
    TB_ASSERT(true, "this should not fire");

    std::printf("Assert tests passed.\n");
    return 0;
}