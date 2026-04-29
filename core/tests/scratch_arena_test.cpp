// core/tests/scratch_arena_test.cpp
//
// Unit tests for ScratchArena — verify the inline-buffer / upstream-spill
// boundary, value-initialisation guarantees, and that the arena releases
// all upstream allocations on destruction.

#include <gtest/gtest.h>

#include <numkit/core/scratch.hpp>

#include <cstddef>
#include <memory_resource>
#include <new>

using numkit::ScratchArena;
using numkit::ScratchVec;

namespace {

// Tracks every allocate/deallocate the upstream memory_resource sees.
// Lets us assert that the inline buffer absorbs small scratches (zero
// upstream hits) and that overflows are eventually returned (matched
// dealloc count). Subclassing std::pmr::memory_resource is the standard
// pmr extension point for embedders who want a custom heap policy.
struct UpstreamCounter : public std::pmr::memory_resource
{
    std::size_t alloc_calls   = 0;
    std::size_t dealloc_calls = 0;
    std::size_t bytes_alive   = 0;

protected:
    void *do_allocate(std::size_t n, std::size_t /*align*/) override
    {
        ++alloc_calls;
        bytes_alive += n;
        return ::operator new(n);
    }
    void do_deallocate(void *p, std::size_t n, std::size_t /*align*/) override
    {
        ++dealloc_calls;
        bytes_alive -= n;
        ::operator delete(p);
    }
    bool do_is_equal(const memory_resource &other) const noexcept override
    {
        return this == &other;
    }
};

} // namespace

// ─── inline buffer absorbs small scratches ──────────────────────────

TEST(ScratchArenaTest, SmallScratchDoesNotTouchUpstream)
{
    UpstreamCounter c;

    {
        ScratchArena scratch(&c);
        ScratchVec<double>      v1(64,  &scratch);   //  512 B
        ScratchVec<int>         v2(128, &scratch);   //  512 B
        ScratchVec<std::size_t> v3(64,  &scratch);   //  512 B
        // total ≈ 1.5 KiB ≪ inline buffer

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

    {
        ScratchArena scratch(&c);
        // 10 000 doubles = 80 000 B — far beyond the inline buffer
        ScratchVec<double> v(10'000, &scratch);
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

// ─── ScratchVec<T>(n, mr) value-initialises arithmetic elements to zero

TEST(ScratchArenaTest, VecValueInitialisesArithmeticToZero)
{
    ScratchArena scratch(std::pmr::get_default_resource());

    ScratchVec<double> vd(16, &scratch);
    for (double x : vd)
        EXPECT_DOUBLE_EQ(x, 0.0);

    ScratchVec<int> vi(16, &scratch);
    for (int x : vi)
        EXPECT_EQ(x, 0);

    ScratchVec<std::size_t> vs(16, &scratch);
    for (std::size_t x : vs)
        EXPECT_EQ(x, 0u);
}

// ─── ScratchVec<T>(mr) (no size) returns empty vector that can grow ─

TEST(ScratchArenaTest, VecDefaultIsEmptyAndGrowable)
{
    ScratchArena scratch(std::pmr::get_default_resource());

    ScratchVec<double> v(&scratch);
    EXPECT_EQ(v.size(), 0u);

    v.reserve(8);
    for (int i = 0; i < 8; ++i)
        v.push_back(static_cast<double>(i) * 0.5);

    EXPECT_EQ(v.size(), 8u);
    EXPECT_DOUBLE_EQ(v[3], 1.5);
    EXPECT_DOUBLE_EQ(v[7], 3.5);
}

// ─── alloc-aware copy ctor attaches the target arena, not default ───
//
// Regression guard: pmr::vector's IMPLICIT copy ctor uses
// select_on_container_copy_construction → default_resource. ScratchVec
// deletes that implicit copy at compile time. The remaining explicit
// alloc-aware copy ctor (inherited via `using base_type::base_type`)
// stays — and this test verifies it actually attaches the supplied
// resource rather than silently falling through to default.
//
// To make this test *actually* catch a regression, src is sized to
// overflow the inline buffer. A correct ctor spills into the counted
// upstream (alloc_calls > 0). A broken ctor that used default_resource
// would allocate on the default heap and leave upstream untouched
// (alloc_calls == 0) — caught by EXPECT_GT below.

TEST(ScratchArenaTest, AllocAwareCopyRoutesThroughArena)
{
    UpstreamCounter c;

    // 32768 ints = 128 KB — far beyond the inline buffer.
    std::pmr::vector<int> src(32768, 42);

    {
        ScratchArena scratch(&c);
        ScratchVec<int> dst(src, &scratch);
        EXPECT_EQ(dst.size(), 32768u);
        for (int v : dst)
            EXPECT_EQ(v, 42);
        EXPECT_GT(c.alloc_calls, 0u)
            << "ScratchVec(other, mr) must route through the arena "
               "(which spills to counted upstream when overflowing inline buffer)";
    }
    EXPECT_EQ(c.alloc_calls, c.dealloc_calls);
    EXPECT_EQ(c.bytes_alive, 0u);
}

// ─── multiple arenas in same scope are independent ──────────────────

TEST(ScratchArenaTest, MultipleArenasAreIndependent)
{
    UpstreamCounter c;

    {
        ScratchArena s1(&c);
        ScratchArena s2(&c);

        ScratchVec<int> v1(32, &s1);
        ScratchVec<int> v2(32, &s2);
        v1[0] = 100;
        v2[0] = 200;
        EXPECT_EQ(v1[0], 100);
        EXPECT_EQ(v2[0], 200);
    }

    EXPECT_EQ(c.alloc_calls, 0u);
    EXPECT_EQ(c.bytes_alive, 0u);
}
