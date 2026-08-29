// src/core/tests/stack_guard_test.cpp
//
// Mechanism-level test for the StackGuard watermark (STACK_SAFETY.md layer 2).
// The parse-depth limit (layer 1) makes the watermark unreachable through
// normal user input on roomy stacks, so prove the mechanism directly: a
// recursion that burns real stack frames while checking the guard must be
// converted into a clean std::runtime_error BEFORE the hardware guard page
// kills the process. Without the guard this test is a stack-overflow crash.

#include <numkit/core/stack_guard.hpp>
#include <gtest/gtest.h>

#include <cstdio>

namespace {

// Burns ~a few hundred bytes of real stack per level. The post-recursion
// read of `pad` keeps the frame from being tail-call-eliminated.
void recurseWithGuard(int depth, int limit)
{
    numkit::StackGuard::check("stack_guard_test recursion");
    volatile char pad[256];
    pad[0] = static_cast<char>(depth & 0x7f);
    if (depth < limit)
        recurseWithGuard(depth + 1, limit);
    if (pad[0] == static_cast<char>(0x7f))
        std::printf("."); // never taken: depth & 0x7f == 0x7f only at 127, 255, ...
}

} // namespace

// The watermark must fire (clean throw) before the actual overflow.
TEST(StackGuardTest, FiresBeforeActualOverflow)
{
    // The limit is far beyond any real stack (~1 MB / ~300 B per level is a
    // few thousand levels); reaching it means the guard never fired.
    EXPECT_THROW(recurseWithGuard(0, 5'000'000), std::runtime_error);
}

// Ordinary recursion depth must not trip the guard.
TEST(StackGuardTest, ShallowRecursionDoesNotTrip)
{
    EXPECT_NO_THROW(recurseWithGuard(0, 64));
}
