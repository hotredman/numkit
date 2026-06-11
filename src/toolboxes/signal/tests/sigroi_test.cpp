// toolboxes/signal/tests/sigroi_test.cpp
//
// Regression guard for the Signal Processing Toolbox ROI utilities:
//   binmask2sigroi / sigroi2binmask / extendsigroi / shortensigroi /
//   mergesigroi / removesigroi / extractsigroi / sigrangebinmask.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SigroiTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── binmask2sigroi ────────────────────────────────────────────────────
TEST_F(SigroiTest, BinMaskToRoiPairs)
{
    eval("r = binmask2sigroi(logical([0 1 1 0 0 1 1 1 0 1]));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 2)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("r(1, 1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(1, 2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3, 1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3, 2)"), 10.0);
}

TEST_F(SigroiTest, BinMaskAllZeroEmpty)
{
    eval("r = binmask2sigroi(logical([0 0 0 0]));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 1)")), 0);
}

TEST_F(SigroiTest, BinMaskAllOneSingleRoi)
{
    eval("r = binmask2sigroi(logical([1 1 1 1]));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 1)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("r(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(1, 2)"), 4.0);
}

// ── sigroi2binmask ────────────────────────────────────────────────────
TEST_F(SigroiTest, RoiToMaskAutoLength)
{
    eval("m = sigroi2binmask([2 3; 6 8; 10 10]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(m)")), 10);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(3))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(7))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(10))"), 1.0);
}

TEST_F(SigroiTest, RoiToMaskExplicitLength)
{
    eval("m = sigroi2binmask([2 3; 6 8; 10 10], 12);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(m)")), 12);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(11))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m(12))"), 0.0);
}

// ── extendsigroi / shortensigroi ──────────────────────────────────────
TEST_F(SigroiTest, ExtendClampsAtOne)
{
    eval("e = extendsigroi([3 5; 8 10], 2, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("e(1, 1)"), 1.0);  // 3-2 = 1
    EXPECT_DOUBLE_EQ(evalScalar("e(1, 2)"), 6.0);  // 5+1
    EXPECT_DOUBLE_EQ(evalScalar("e(2, 1)"), 6.0);  // 8-2
    EXPECT_DOUBLE_EQ(evalScalar("e(2, 2)"), 11.0); // 10+1
}

TEST_F(SigroiTest, ShortenDropsCollapsed)
{
    // [1 10] shortened by (2, 3) → [3 7]; [12 14] shortened by (2, 3)
    // collapses (12+2 > 14-3 → 14 > 11 → still valid: [14 11]? actually
    // 12+2=14, 14-3=11, s>e → drop).
    eval("s = shortensigroi([1 10; 12 14], 2, 3);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(s, 1)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("s(1, 1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(1, 2)"), 7.0);
}

// ── mergesigroi ───────────────────────────────────────────────────────
TEST_F(SigroiTest, MergeNoSepKeepsGaps)
{
    // sep=0: only overlapping merge.
    eval("m = mergesigroi([1 3; 4 6; 8 10; 9 12], 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(m, 1)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 2)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2, 1)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2, 2)"), 12.0);
}

TEST_F(SigroiTest, MergeWithSepCollapses)
{
    // sep=1: even gap-of-1 merges. All 4 collapse to [1 12].
    eval("m = mergesigroi([1 3; 4 6; 8 10; 9 12], 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(m, 1)")), 1);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 2)"), 12.0);
}

// ── removesigroi ──────────────────────────────────────────────────────
TEST_F(SigroiTest, RemoveByMaxLength)
{
    // [1 5] len=5, [7 7] len=1, [10 12] len=3.
    // maxLen=2 drops only [7 7].
    eval("r = removesigroi([1 5; 7 7; 10 12], 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 1)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("r(1, 1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(1, 2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(2, 1)"), 10.0);
}

TEST_F(SigroiTest, RemoveZeroMaxLenKeepsAll)
{
    // maxLen=0 → no ROI has length ≤ 0, all kept.
    eval("r = removesigroi([1 3; 5 7; 10 12], 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(r, 1)")), 3);
}

// ── extractsigroi ─────────────────────────────────────────────────────
TEST_F(SigroiTest, ExtractConcat)
{
    eval("s = extractsigroi((1:20)', [3 5; 8 10], true);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(s)")), 6);
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(4)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(6)"), 10.0);
}

TEST_F(SigroiTest, ExtractCellDefault)
{
    eval("c = extractsigroi((1:20)', [3 5; 8 10]);");
    // Default returns cell array; iscell(c) should be true.
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(c))"), 1.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 2);
}

// ── sigrangebinmask ───────────────────────────────────────────────────
TEST_F(SigroiTest, SigrangeBinmaskScalarAbove)
{
    // bound is scalar → x > bound (default 'above')
    eval("m = sigrangebinmask([1 2 3 4 5 4 3 2 1], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(m(1))"), 0.0);  // 1 > 2 → false
    EXPECT_DOUBLE_EQ(evalScalar("double(m(3))"), 1.0);  // 3 > 2 → true
    EXPECT_DOUBLE_EQ(evalScalar("double(m(5))"), 1.0);  // 5 > 2
}

TEST_F(SigroiTest, SigrangeBinmaskTwoVecInside)
{
    // bound is 2-vec → vmin <= x <= vmax (default 'inside' closed)
    eval("m = sigrangebinmask([1 2 3 4 5 4 3 2 1], [2 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(m(1))"), 0.0);  // 1 not in [2,4]
    EXPECT_DOUBLE_EQ(evalScalar("double(m(2))"), 1.0);  // 2 in
    EXPECT_DOUBLE_EQ(evalScalar("double(m(4))"), 1.0);  // 4 in
    EXPECT_DOUBLE_EQ(evalScalar("double(m(5))"), 0.0);  // 5 not in
}
