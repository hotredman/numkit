// toolboxes/builtin/tests/minmax_nan_test.cpp
//
// Regression guard: two-argument max(a,b) / min(a,b) ignore NaN like
// MATLAB R2025b (return the non-NaN operand; NaN only if both are NaN),
// on BOTH engines and for both scalar (fast path) and array (Value-level)
// inputs.
//
// Bug history: the VM scalar fast path used (a>=b)?a:b, returning NaN for
// max(5,NaN) and disagreeing with the TreeWalker (which used fmax). The
// Value-level reductions.cpp used std::max/std::min, which are NaN-order-
// dependent and returned NaN for max([NaN ..], ..). Fixed by using
// std::fmax/std::fmin in both places.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class MinMaxNanTest : public DualEngineTest {};

TEST_P(MinMaxNanTest, ScalarBinaryIgnoresNaN)
{
    EXPECT_DOUBLE_EQ(evalScalar("max(5, NaN)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(NaN, 5)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(5, NaN)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(NaN, 5)"), 5.0);
    EXPECT_TRUE(evalBool("isnan(max(NaN, NaN))"));
    EXPECT_TRUE(evalBool("isnan(min(NaN, NaN))"));
    EXPECT_DOUBLE_EQ(evalScalar("max(-Inf, 5)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(Inf, 5)"),  5.0);
}

TEST_P(MinMaxNanTest, ArrayBinaryIgnoresNaN)
{
    // both orderings (NaN first and second) + both-NaN.
    eval("u = max([NaN 6 NaN 8 NaN],[5 7 9 NaN NaN]);");
    EXPECT_DOUBLE_EQ(evalScalar("u(1)"), 5.0); // max(NaN,5)
    EXPECT_DOUBLE_EQ(evalScalar("u(2)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(3)"), 9.0); // max(NaN,9)
    EXPECT_DOUBLE_EQ(evalScalar("u(4)"), 8.0); // max(8,NaN)
    EXPECT_TRUE(evalBool("isnan(u(5))"));      // both NaN
    eval("w = min([NaN 6 NaN 8],[5 7 9 NaN]);");
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 5.0); // min(NaN,5)
    EXPECT_DOUBLE_EQ(evalScalar("w(4)"), 8.0); // min(8,NaN)
}

TEST_P(MinMaxNanTest, NormalCasesUnchanged)
{
    EXPECT_DOUBLE_EQ(evalScalar("max(3, 7)"),   7.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(3, 7)"),   3.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(-2, -5)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(-2, -5)"), -5.0);
    eval("a = max([1 8 3],[5 2 9]);"); // [5 8 9]
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(3)"), 9.0);
}

TEST_P(MinMaxNanTest, ReductionDefaultOmitsNaN)
{
    // MATLAB's DEFAULT for max/min over an array is 'omitnan': NaN is skipped
    // (result is NaN only if every element is NaN), and the index points to
    // the extremum among the non-NaN values. The reduction path used to be
    // NaN-order-dependent (max([NaN 3 1]) -> NaN, idx 1).
    eval("[v, i] = max([NaN 3 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("v"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("i"), 2.0);
    eval("[w, j] = min([NaN 3 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("w"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("j"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("max([3 NaN 1])"), 3.0);
    EXPECT_TRUE(evalBool("isnan(max([NaN NaN]))"));   // all-NaN -> NaN
    EXPECT_EQ(static_cast<int>(evalScalar("numel(max([]))")), 0);  // empty stays []
    // Matrix: per-column omit.
    eval("M = max([NaN 2; 3 NaN]);");
    EXPECT_DOUBLE_EQ(evalScalar("M(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("M(2)"), 2.0);
}

TEST_P(MinMaxNanTest, ReductionOmitNanFlagAndIncludeNan)
{
    EXPECT_DOUBLE_EQ(evalScalar("max([3 NaN 1],[],'omitnan')"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("min([3 NaN 1],[],'omitnan')"), 1.0);
    // 'includenan' propagates NaN (leading-NaN case).
    EXPECT_TRUE(evalBool("isnan(max([NaN 3 1],[],'includenan'))"));
}

INSTANTIATE_DUAL(MinMaxNanTest);
