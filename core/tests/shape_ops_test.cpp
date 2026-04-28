// core/tests/shape_ops_test.cpp
//
// Unit tests for ND shape helpers in shape_ops.hpp: broadcasting,
// strides, coord-walking. Direct C++ tests (not driven through the
// dual-engine VM/TreeWalker fixture) — they exercise pure functions
// on Dims and don't need an interpreter.

#include <gtest/gtest.h>

#include <numkit/core/dims.hpp>
#include <numkit/core/shape_ops.hpp>

#include <array>
#include <vector>

using numkit::Dims;
using numkit::broadcastDimsND;
using numkit::broadcastOffsetND;
using numkit::computeStridesColMajor;
using numkit::incrementCoords;
using numkit::linearizeFromCoords;

// ─── broadcastDimsND ────────────────────────────────────────────

TEST(ShapeOpsTest, BroadcastSameShape2D)
{
    Dims a(3, 4), b(3, 4), out;
    EXPECT_TRUE(broadcastDimsND(a, b, out));
    EXPECT_EQ(out.ndim(), 2);
    EXPECT_EQ(out.rows(), 3u);
    EXPECT_EQ(out.cols(), 4u);
}

TEST(ShapeOpsTest, BroadcastRowVsScalar)
{
    Dims a(1, 5), b(1, 1), out;
    EXPECT_TRUE(broadcastDimsND(a, b, out));
    EXPECT_EQ(out.rows(), 1u);
    EXPECT_EQ(out.cols(), 5u);
}

TEST(ShapeOpsTest, BroadcastIncompatible)
{
    Dims a(3, 4), b(2, 4), out;
    EXPECT_FALSE(broadcastDimsND(a, b, out));
}

TEST(ShapeOpsTest, Broadcast3DAgainst2D)
{
    Dims a(3, 4, 2), b(3, 4), out;
    EXPECT_TRUE(broadcastDimsND(a, b, out));
    EXPECT_EQ(out.ndim(), 3);
    EXPECT_EQ(out.rows(), 3u);
    EXPECT_EQ(out.cols(), 4u);
    EXPECT_EQ(out.pages(), 2u);
}

TEST(ShapeOpsTest, BroadcastND4D)
{
    const size_t da[] = {2, 3, 1, 5};
    const size_t db[] = {2, 1, 4, 5};
    Dims a(da, 4), b(db, 4), out;
    EXPECT_TRUE(broadcastDimsND(a, b, out));
    EXPECT_EQ(out.ndim(), 4);
    EXPECT_EQ(out.dim(0), 2u);
    EXPECT_EQ(out.dim(1), 3u);
    EXPECT_EQ(out.dim(2), 4u);
    EXPECT_EQ(out.dim(3), 5u);
}

TEST(ShapeOpsTest, BroadcastND5DAgainst2D)
{
    const size_t da[] = {2, 3, 4, 5, 6};
    Dims a(da, 5), b(2, 3), out;
    EXPECT_TRUE(broadcastDimsND(a, b, out));
    EXPECT_EQ(out.ndim(), 5);
    EXPECT_EQ(out.dim(0), 2u);
    EXPECT_EQ(out.dim(1), 3u);
    EXPECT_EQ(out.dim(2), 4u);
    EXPECT_EQ(out.dim(3), 5u);
    EXPECT_EQ(out.dim(4), 6u);
}

// ─── broadcastOffsetND ───────────────────────────────────────────

TEST(ShapeOpsTest, BroadcastOffsetIdentity2D)
{
    Dims op(3, 4);
    std::array<size_t, 2> coords = {2, 3};
    // 2D op, output shape same as op → offset = c*R + r = 3*3 + 2 = 11.
    EXPECT_EQ(broadcastOffsetND(coords.data(), 2, op), 11u);
}

TEST(ShapeOpsTest, BroadcastOffsetRowBroadcast)
{
    // op is shape (1, 4). For coord (rr=2, cc=3), broadcast collapses
    // rr → 0, so offset = 3 * 1 + 0 = 3.
    Dims op(1, 4);
    std::array<size_t, 2> coords = {2, 3};
    EXPECT_EQ(broadcastOffsetND(coords.data(), 2, op), 3u);
}

TEST(ShapeOpsTest, BroadcastOffset3DPageCollapse)
{
    // op is shape (3, 4, 1). For coord (1, 2, 7), broadcast collapses
    // p → 0. Linear offset = 0*12 + 2*3 + 1 = 7.
    Dims op(3, 4, 1);
    std::array<size_t, 3> coords = {1, 2, 7};
    EXPECT_EQ(broadcastOffsetND(coords.data(), 3, op), 7u);
}

TEST(ShapeOpsTest, BroadcastOffsetMissingTrailingDim)
{
    // op is shape (3, 4) [ndim=2]. Output is ndim=3 (e.g. broadcast
    // against a 3D operand). Coord pp is ignored (treated as collapsed
    // since op has implicit trailing 1 at axis 2).
    Dims op(3, 4);
    std::array<size_t, 3> coords = {1, 2, 99};
    EXPECT_EQ(broadcastOffsetND(coords.data(), 3, op), 7u);
}

// ─── computeStridesColMajor ──────────────────────────────────────

TEST(ShapeOpsTest, StridesColMajor2D)
{
    Dims d(3, 4);
    std::array<size_t, 2> s{};
    computeStridesColMajor(d, s.data());
    EXPECT_EQ(s[0], 1u);
    EXPECT_EQ(s[1], 3u);
}

TEST(ShapeOpsTest, StridesColMajor3D)
{
    Dims d(3, 4, 5);
    std::array<size_t, 3> s{};
    computeStridesColMajor(d, s.data());
    EXPECT_EQ(s[0], 1u);
    EXPECT_EQ(s[1], 3u);
    EXPECT_EQ(s[2], 12u);
}

TEST(ShapeOpsTest, StridesColMajor5D)
{
    const size_t dims[] = {2, 3, 4, 5, 6};
    Dims d(dims, 5);
    std::array<size_t, 5> s{};
    computeStridesColMajor(d, s.data());
    EXPECT_EQ(s[0], 1u);
    EXPECT_EQ(s[1], 2u);
    EXPECT_EQ(s[2], 6u);
    EXPECT_EQ(s[3], 24u);
    EXPECT_EQ(s[4], 120u);
}

// ─── linearizeFromCoords ─────────────────────────────────────────

TEST(ShapeOpsTest, LinearizeMatchesDirectFormula3D)
{
    Dims d(3, 4, 5);
    std::array<size_t, 3> s{};
    computeStridesColMajor(d, s.data());
    std::array<size_t, 3> coords = {2, 3, 4};
    // Expected: 4*3*4 + 3*3 + 2 = 48 + 9 + 2 = 59.
    EXPECT_EQ(linearizeFromCoords(coords.data(), s.data(), 3), 59u);
}

// ─── incrementCoords ─────────────────────────────────────────────

TEST(ShapeOpsTest, IncrementCoords2DCoversAllPositions)
{
    Dims d(3, 4);
    std::array<size_t, 2> coords = {0, 0};
    size_t visited = 1;  // counts the starting position
    while (incrementCoords(coords.data(), d)) ++visited;
    EXPECT_EQ(visited, d.numel());
}

TEST(ShapeOpsTest, IncrementCoords3DCarryAndOrder)
{
    Dims d(2, 3, 2);
    std::array<size_t, 3> coords = {0, 0, 0};
    // Expected sequence: r varies fastest, then c, then p.
    std::vector<std::array<size_t, 3>> got;
    got.push_back(coords);
    while (incrementCoords(coords.data(), d)) got.push_back(coords);
    ASSERT_EQ(got.size(), d.numel());
    EXPECT_EQ(got[0],  (std::array<size_t, 3>{0, 0, 0}));
    EXPECT_EQ(got[1],  (std::array<size_t, 3>{1, 0, 0}));
    EXPECT_EQ(got[2],  (std::array<size_t, 3>{0, 1, 0}));
    EXPECT_EQ(got[3],  (std::array<size_t, 3>{1, 1, 0}));
    EXPECT_EQ(got[6],  (std::array<size_t, 3>{0, 0, 1}));
    EXPECT_EQ(got[11], (std::array<size_t, 3>{1, 2, 1}));
}

TEST(ShapeOpsTest, IncrementCoords5DCount)
{
    const size_t dims[] = {2, 3, 4, 5, 6};
    Dims d(dims, 5);
    std::array<size_t, 5> coords{};
    size_t visited = 1;
    while (incrementCoords(coords.data(), d)) ++visited;
    EXPECT_EQ(visited, d.numel());  // 720
}

// ─── Dims SBO sanity ─────────────────────────────────────────────

TEST(ShapeOpsTest, DimsND5DStorageRoundtrip)
{
    const size_t in[] = {2, 3, 4, 5, 6};
    Dims d(in, 5);
    EXPECT_EQ(d.ndim(), 5);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(d.dim(i), in[i]) << "axis " << i;
    EXPECT_EQ(d.numel(), 720u);
}

TEST(ShapeOpsTest, DimsND6DCopyMovePreservesContent)
{
    const size_t in[] = {2, 3, 4, 5, 6, 7};
    Dims d1(in, 6);
    Dims d2 = d1;          // copy
    EXPECT_EQ(d1, d2);
    Dims d3 = std::move(d1); // move
    EXPECT_EQ(d3, d2);
    for (int i = 0; i < 6; ++i)
        EXPECT_EQ(d3.dim(i), in[i]);
}

TEST(ShapeOpsTest, DimsEqualityIgnoresTrailingSingletons)
{
    Dims a(3, 4);
    Dims b(3, 4, 1);
    EXPECT_EQ(a, b);
    Dims c(3, 4, 2);
    EXPECT_NE(a, c);
}

TEST(ShapeOpsTest, DimsNDTrailingSingletonNormalisation)
{
    const size_t base[] = {3, 4};
    const size_t pad[]  = {3, 4, 1, 1, 1};
    EXPECT_EQ(Dims(base, 2), Dims(pad, 5));
}
