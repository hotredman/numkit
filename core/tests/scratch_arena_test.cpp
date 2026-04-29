// core/tests/scratch_arena_test.cpp
//
// Unit tests for ScratchArena — verify the inline-buffer / upstream-spill
// boundary, value-initialisation guarantees of vec<T>(n), and that the
// arena releases all upstream allocations on destruction.

#include <gtest/gtest.h>

#include <numkit/core/allocator.hpp>
#include <numkit/core/scratch_arena.hpp>

#include <cstddef>
#include <new>

using numkit::Allocator;
using numkit::ScratchArena;
using numkit::ScratchVec;

namespace {

// Tracks every allocate/deallocate the upstream Allocator sees. Letting
// us assert that the inline buffer absorbs small scratches (zero upstream
// hits) and that overflows are eventually returned (matched dealloc count).
struct UpstreamCounter
{
    std::size_t alloc_calls   = 0;
    std::size_t dealloc_calls = 0;
    std::size_t bytes_alive   = 0;

    Allocator make()
    {
        Allocator a;
        a.allocate = [this](std::size_t n) -> void * {
            ++alloc_calls;
            bytes_alive += n;
            return ::operator new(n);
        };
        a.deallocate = [this](void *p, std::size_t n) {
            ++dealloc_calls;
            bytes_alive -= n;
            ::operator delete(p);
        };
        return a;
    }
};

} // namespace

// ─── inline buffer absorbs small scratches ──────────────────────────

TEST(ScratchArenaTest, SmallScratchDoesNotTouchUpstream)
{
    UpstreamCounter c;
    Allocator a = c.make();

    {
        ScratchArena scratch(a);
        auto v1 = scratch.vec<double>(64);   //  512 B
        auto v2 = scratch.vec<int>(128);     //  512 B
        auto v3 = scratch.vec<std::size_t>(64); //  512 B
        // total ≈ 1.5 KiB ≪ 4 KiB inline buffer

        EXPECT_EQ(v1.size(), 64u);
        EXPECT_EQ(v2.size(), 128u);
        EXPECT_EQ(v3.size(), 64u);
    }

    EXPECT_EQ(c.alloc_calls, 0u) << "small scratches should hit only the inline buffer";
    EXPECT_EQ(c.dealloc_calls, 0u);
    EXPECT_EQ(c.bytes_alive, 0u);
}

// ─── overflow spills to upstream and is fully released ──────────────

TEST(ScratchArenaTest, LargeScratchSpillsToUpstreamAndReleases)
{
    UpstreamCounter c;
    Allocator a = c.make();

    {
        ScratchArena scratch(a);
        // 10 000 doubles = 80 000 B — far beyond the 4 KiB inline buffer
        auto v = scratch.vec<double>(10'000);
        EXPECT_EQ(v.size(), 10'000u);
        v[0]    = 1.0;
        v.back() = 2.0;
        EXPECT_DOUBLE_EQ(v[0], 1.0);
        EXPECT_DOUBLE_EQ(v.back(), 2.0);
        EXPECT_GT(c.alloc_calls, 0u) << "overflow must reach upstream";
    }

    EXPECT_EQ(c.alloc_calls, c.dealloc_calls) << "arena must release every upstream chunk on scope exit";
    EXPECT_EQ(c.bytes_alive, 0u);
}

// ─── vec<T>(n) value-initialises arithmetic elements to zero ────────

TEST(ScratchArenaTest, VecValueInitialisesArithmeticToZero)
{
    Allocator a = Allocator::defaultAllocator();
    ScratchArena scratch(a);

    auto vd = scratch.vec<double>(16);
    for (double x : vd)
        EXPECT_DOUBLE_EQ(x, 0.0);

    auto vi = scratch.vec<int>(16);
    for (int x : vi)
        EXPECT_EQ(x, 0);

    auto vs = scratch.vec<std::size_t>(16);
    for (std::size_t x : vs)
        EXPECT_EQ(x, 0u);
}

// ─── vec<T>() returns empty vector that can grow ────────────────────

TEST(ScratchArenaTest, VecDefaultIsEmptyAndGrowable)
{
    Allocator a = Allocator::defaultAllocator();
    ScratchArena scratch(a);

    auto v = scratch.vec<double>();
    EXPECT_EQ(v.size(), 0u);

    v.reserve(8);
    for (int i = 0; i < 8; ++i)
        v.push_back(static_cast<double>(i) * 0.5);

    EXPECT_EQ(v.size(), 8u);
    EXPECT_DOUBLE_EQ(v[3], 1.5);
    EXPECT_DOUBLE_EQ(v[7], 3.5);
}

// ─── scratchCopyOf attaches the target arena, not default ───────────
//
// Regression guard: pmr::vector's copy ctor uses
// select_on_container_copy_construction → default_resource. Without
// the explicit-allocator copy, X would silently allocate off-arena.
//
// To make this test *actually* catch a regression, src is sized to
// overflow the 4 KiB inline buffer. A correct helper spills into the
// counted upstream (alloc_calls > 0). A broken helper that uses
// default_resource would allocate on the default heap and leave
// upstream untouched (alloc_calls == 0) — caught by EXPECT_GT below.

TEST(ScratchArenaTest, ScratchCopyOfRoutesThroughArena)
{
    UpstreamCounter c;
    Allocator a = c.make();

    // 2048 ints = 8 KB — overflows the 4 KiB inline buffer.
    std::pmr::vector<int> src(2048, 42);

    {
        ScratchArena scratch(a);
        auto dst = numkit::scratchCopyOf(scratch.resource(), src);
        EXPECT_EQ(dst.size(), 2048u);
        for (int v : dst)
            EXPECT_EQ(v, 42);
        EXPECT_GT(c.alloc_calls, 0u)
            << "scratchCopyOf must route through the arena (which "
               "spills to counted upstream when overflowing inline buffer)";
    }
    EXPECT_EQ(c.alloc_calls, c.dealloc_calls);
    EXPECT_EQ(c.bytes_alive, 0u);
}

// ─── multiple arenas in same scope are independent ──────────────────

TEST(ScratchArenaTest, MultipleArenasAreIndependent)
{
    UpstreamCounter c;
    Allocator a = c.make();

    {
        ScratchArena s1(a);
        ScratchArena s2(a);

        auto v1 = s1.vec<int>(32);
        auto v2 = s2.vec<int>(32);
        v1[0] = 100;
        v2[0] = 200;
        EXPECT_EQ(v1[0], 100);
        EXPECT_EQ(v2[0], 200);
    }

    EXPECT_EQ(c.alloc_calls, 0u);
    EXPECT_EQ(c.bytes_alive, 0u);
}
