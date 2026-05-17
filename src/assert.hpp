#pragma once
// tangbase::panic(msg) — prints to stderr and aborts the program
// TB_CHECK(cond)       — compile-time static_assert (condition must be true)
// TB_ASSERT(cond, msg) — runtime assertion; calls panic() in both Debug and Release

#include <cstdio>
#include <cstdlib>

namespace tangbase {

// Print msg to stderr and terminate the process immediately.
// Use this for truly unrecoverable states (e.g., violated invariant).
[[noreturn]] void panic(const char* msg) {
    std::fputs(msg, stderr);
    std::fputs("\n", stderr);
    std::abort();
}

}  // namespace tangbase

// Compile-time assertion. Fails with the condition's source text as the message.
// Use TB_CHECK when the condition must be true at compile time (type constraints, etc.).
#define TB_CHECK(cond) static_assert(cond, #cond)

// Runtime assertion. Evaluates cond; if false, calls tangbase::panic(msg).
// Unlike standard assert(), TB_ASSERT is active in Release builds.
#define TB_ASSERT(cond, msg)                                                     \
    ((cond) ? void(0) : ::tangbase::panic(msg))