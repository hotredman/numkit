// libs/signal/tests/buffer_test.cpp
//
// Regression guard for signal/buffer (Phase 4.1 of audio extension sweep).
// Bit-equal MATLAB R2025b on all 6 documented variants.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BufferTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── nargin == 2: simple non-overlap with zero-pad ─────────────────────
TEST_F(BufferTest, NoOverlapZeroPadLast)
{
    eval("y = buffer(1:18, 8);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 3);
    // First column = [1..8], second = [9..16], third = [17, 18, 0, 0, 0, 0, 0, 0]
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8, 2)"), 16.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 3)"), 17.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2, 3)"), 18.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8, 3)"), 0.0);  // zero-padded
}

// ── nargin == 3, p > 0: overlap with initial p zeros ──────────────────
TEST_F(BufferTest, OverlapWithInitialZeros)
{
    eval("y = buffer(1:18, 8, 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 5);
    // First col starts with 4 zeros (initial condition).
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4, 1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8, 1)"), 4.0);
    // Second col overlaps with last 4 of first.
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8, 2)"), 8.0);
    // Last col has zero-pad at end.
    EXPECT_DOUBLE_EQ(evalScalar("y(7, 5)"), 0.0);
}

// ── nargin == 4 with 'nodelay': overlap WITHOUT initial zeros ─────────
TEST_F(BufferTest, NoDelayOverlap)
{
    eval("y = buffer(1:18, 8, 4, 'nodelay');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 4);
    // First col = [1..8] directly (no initial zeros).
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8, 1)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 2)"), 5.0);  // overlap
}

// ── nargin == 3, p < 0: underlap (skip samples between frames) ────────
TEST_F(BufferTest, UnderlapSkipsSamples)
{
    eval("y = buffer(1:24, 8, -4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 2);
    // First col = [1..8], second = [13..20] (skip 9..12)
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8, 1)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 2)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(8, 2)"), 20.0);
}

// ── 2-output [Y, Z]: complete frames + partial-frame remainder ────────
TEST_F(BufferTest, TwoOutputCompleteAndPartial)
{
    eval("[y, z] = buffer(1:18, 8);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 2);  // complete frames only
    EXPECT_EQ(static_cast<int>(evalScalar("numel(z)")), 2);    // [17, 18]
    EXPECT_DOUBLE_EQ(evalScalar("z(1)"), 17.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(2)"), 18.0);
    // Z preserves orientation: row input → row z (1 × 2).
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 2)")), 2);
}

// ── column-vector input ────────────────────────────────────────────────
TEST_F(BufferTest, ColumnVectorInput)
{
    eval("y = buffer((1:10)', 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4, 3)"), 0.0);  // zero-pad
}

// ── orientation of Z matches X (column input → column z) ──────────────
TEST_F(BufferTest, TwoOutputZOrientationMatchesColumn)
{
    eval("[y, z] = buffer((1:18)', 8);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 1)")), 2);  // col vec
    EXPECT_EQ(static_cast<int>(evalScalar("size(z, 2)")), 1);
}

// ── error cases ────────────────────────────────────────────────────────
TEST_F(BufferTest, RejectsOverlapEqualToFrameLen)
{
    bool threw = false;
    try { eval("buffer(1:10, 8, 8);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
