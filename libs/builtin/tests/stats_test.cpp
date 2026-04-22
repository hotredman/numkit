// libs/builtin/tests/stats_test.cpp
//
// Phase-1 statistics: var, std, median, quantile, prctile, mode.
// Tests run on both TreeWalker and VM backends via DualEngineTest.
//
// Coverage matrix per function:
//   - Scalar input
//   - Empty input (where defined)
//   - 1D row vector
//   - 1D column vector
//   - 2D matrix with default dim
//   - 2D matrix with explicit dim=1, dim=2
//   - 3D matrix with dim=1, dim=2, dim=3
// Plus function-specific edge cases (normalization flag, quantile
// boundaries, mode tie-breaking, etc.). Ground-truth values come from
// MATLAB R2024b for the non-trivial cases.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class StatsTest : public DualEngineTest
{};

// ============================================================
// var
// ============================================================

TEST_P(StatsTest, VarVectorDefault)
{
    // var([1 2 3 4 5]) == 2.5 (sample variance, N-1 normalization)
    EXPECT_NEAR(evalScalar("var([1 2 3 4 5]);"), 2.5, 1e-12);
}

TEST_P(StatsTest, VarVectorPopulationFlag)
{
    // var([1 2 3 4 5], 1) == 2.0 (population, N normalization)
    EXPECT_NEAR(evalScalar("var([1 2 3 4 5], 1);"), 2.0, 1e-12);
}

TEST_P(StatsTest, VarScalar)
{
    // var(5) — single element with N-1 normalization is NaN (matches MATLAB).
    auto v = eval("var(5);");
    EXPECT_TRUE(std::isnan(v.toScalar()));
    // var(5, 1) with population normalization is 0.
    EXPECT_DOUBLE_EQ(evalScalar("var(5, 1);"), 0.0);
}

TEST_P(StatsTest, VarColumnVector)
{
    EXPECT_NEAR(evalScalar("var([1; 2; 3; 4; 5]);"), 2.5, 1e-12);
}

TEST_P(StatsTest, VarMatrixDefaultDim)
{
    // var of 2D matrix reduces along columns → row vector.
    eval("v = var([1 2 3; 4 5 6; 7 8 9]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 3u);
    // each column [1,4,7], [2,5,8], [3,6,9] has var 9.0
    EXPECT_NEAR(v->doubleData()[0], 9.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[1], 9.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[2], 9.0, 1e-12);
}

TEST_P(StatsTest, VarMatrixDim2)
{
    // var(M, 0, 2) reduces rows → column vector.
    eval("v = var([1 2 3; 4 5 6; 7 8 9], 0, 2);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 1u);
    // each row [1,2,3], [4,5,6], [7,8,9] has var 1.0
    EXPECT_NEAR(v->doubleData()[0], 1.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[1], 1.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[2], 1.0, 1e-12);
}

TEST_P(StatsTest, VarMatrixPopulationDim2)
{
    // var(M, 1, 2) — population variance along dim=2.
    eval("v = var([1 2 3; 4 5 6], 1, 2);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 1u);
    // row [1,2,3] population var = ((1-2)^2 + 0 + (3-2)^2)/3 = 2/3
    EXPECT_NEAR(v->doubleData()[0], 2.0/3.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[1], 2.0/3.0, 1e-12);
}

TEST_P(StatsTest, VarBadFlag)
{
    EXPECT_THROW(eval("var([1 2 3], 2);"), std::runtime_error);
}

// ============================================================
// std
// ============================================================

TEST_P(StatsTest, StdVectorDefault)
{
    // std([1 2 3 4 5]) == sqrt(2.5)
    EXPECT_NEAR(evalScalar("std([1 2 3 4 5]);"), std::sqrt(2.5), 1e-12);
}

TEST_P(StatsTest, StdPopulationFlag)
{
    EXPECT_NEAR(evalScalar("std([1 2 3 4 5], 1);"), std::sqrt(2.0), 1e-12);
}

TEST_P(StatsTest, StdMatrixDim1)
{
    eval("v = std([1 2; 3 4; 5 6]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 2u);
    // both columns [1,3,5] and [2,4,6] have std=2
    EXPECT_NEAR(v->doubleData()[0], 2.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[1], 2.0, 1e-12);
}

TEST_P(StatsTest, StdMatrixDim2)
{
    eval("v = std([1 2 3; 4 5 6; 7 8 9], 0, 2);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 1u);
    EXPECT_NEAR(v->doubleData()[0], 1.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[1], 1.0, 1e-12);
    EXPECT_NEAR(v->doubleData()[2], 1.0, 1e-12);
}

// ============================================================
// median
// ============================================================

TEST_P(StatsTest, MedianVectorOdd)
{
    EXPECT_DOUBLE_EQ(evalScalar("median([3 1 4 1 5 9 2]);"), 3.0);
}

TEST_P(StatsTest, MedianVectorEven)
{
    // [1 2 3 4] → average of middle two = 2.5
    EXPECT_DOUBLE_EQ(evalScalar("median([1 2 3 4]);"), 2.5);
}

TEST_P(StatsTest, MedianScalar)
{
    EXPECT_DOUBLE_EQ(evalScalar("median(7);"), 7.0);
}

TEST_P(StatsTest, MedianColumnVector)
{
    EXPECT_DOUBLE_EQ(evalScalar("median([5; 1; 4; 2; 3]);"), 3.0);
}

TEST_P(StatsTest, MedianMatrixDim1)
{
    // median per column
    eval("m = median([1 2 3; 4 5 6; 7 8 9]);");
    auto *m = getVarPtr("m");
    EXPECT_EQ(rows(*m), 1u);
    EXPECT_EQ(cols(*m), 3u);
    EXPECT_DOUBLE_EQ(m->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(m->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(m->doubleData()[2], 6.0);
}

TEST_P(StatsTest, MedianMatrixDim2)
{
    eval("m = median([1 2 3; 4 5 6; 7 8 9], 2);");
    auto *m = getVarPtr("m");
    EXPECT_EQ(rows(*m), 3u);
    EXPECT_EQ(cols(*m), 1u);
    EXPECT_DOUBLE_EQ(m->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(m->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(m->doubleData()[2], 8.0);
}

TEST_P(StatsTest, Median3DDim3)
{
    // 2×2×3 array; reduce along pages.
    eval("A = zeros(2, 2, 3); "
         "A(:,:,1) = [1 2; 3 4]; "
         "A(:,:,2) = [5 6; 7 8]; "
         "A(:,:,3) = [9 10; 11 12]; "
         "m = median(A, 3);");
    auto *m = getVarPtr("m");
    EXPECT_EQ(rows(*m), 2u);
    EXPECT_EQ(cols(*m), 2u);
    // Each (r,c) takes the median of [A(r,c,1), A(r,c,2), A(r,c,3)] = middle one.
    EXPECT_DOUBLE_EQ((*m)(0, 0), 5.0);  // [1, 5, 9]
    EXPECT_DOUBLE_EQ((*m)(1, 0), 7.0);  // [3, 7, 11]
    EXPECT_DOUBLE_EQ((*m)(0, 1), 6.0);  // [2, 6, 10]
    EXPECT_DOUBLE_EQ((*m)(1, 1), 8.0);  // [4, 8, 12]
}

// ============================================================
// quantile / prctile
// ============================================================

TEST_P(StatsTest, QuantileScalarP)
{
    // quantile([1 2 3 4 5], 0.5) == 3 (median equivalence)
    EXPECT_NEAR(evalScalar("quantile([1 2 3 4 5], 0.5);"), 3.0, 1e-12);
}

TEST_P(StatsTest, QuantileBoundaryP)
{
    EXPECT_DOUBLE_EQ(evalScalar("quantile([1 2 3 4 5], 0);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("quantile([1 2 3 4 5], 1);"), 5.0);
}

TEST_P(StatsTest, QuantileLinearInterp)
{
    // For [1 2 3 4 5] with p=0.25: h = 0.25*4 = 1.0 → exactly index 1 → 2.0
    EXPECT_NEAR(evalScalar("quantile([1 2 3 4 5], 0.25);"), 2.0, 1e-12);
    // p=0.1: h = 0.4 → 1 + 0.4*(2-1) = 1.4
    EXPECT_NEAR(evalScalar("quantile([1 2 3 4 5], 0.1);"), 1.4, 1e-12);
}

TEST_P(StatsTest, QuantileVectorP)
{
    // quantile of a vector with vector p → 1×k row vector
    eval("q = quantile([1 2 3 4 5], [0 0.5 1]);");
    auto *q = getVarPtr("q");
    EXPECT_EQ(rows(*q), 1u);
    EXPECT_EQ(cols(*q), 3u);
    EXPECT_DOUBLE_EQ(q->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(q->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(q->doubleData()[2], 5.0);
}

TEST_P(StatsTest, QuantileMatrixDim1)
{
    eval("q = quantile([1 2; 3 4; 5 6], 0.5);");
    auto *q = getVarPtr("q");
    EXPECT_EQ(rows(*q), 1u);
    EXPECT_EQ(cols(*q), 2u);
    EXPECT_DOUBLE_EQ(q->doubleData()[0], 3.0); // median of [1,3,5]
    EXPECT_DOUBLE_EQ(q->doubleData()[1], 4.0); // median of [2,4,6]
}

TEST_P(StatsTest, QuantileMatrixDim2)
{
    eval("q = quantile([1 2 3; 4 5 6], 0.5, 2);");
    auto *q = getVarPtr("q");
    EXPECT_EQ(rows(*q), 2u);
    EXPECT_EQ(cols(*q), 1u);
    EXPECT_DOUBLE_EQ(q->doubleData()[0], 2.0); // median of [1,2,3]
    EXPECT_DOUBLE_EQ(q->doubleData()[1], 5.0); // median of [4,5,6]
}

TEST_P(StatsTest, QuantileVectorPMatrixDim2)
{
    // 2×3 matrix, p = [0.25, 0.75], dim=2 → output is 2×2.
    eval("q = quantile([1 2 3; 10 20 30], [0.25 0.75], 2);");
    auto *q = getVarPtr("q");
    EXPECT_EQ(rows(*q), 2u);
    EXPECT_EQ(cols(*q), 2u);
    // row [1,2,3]: q(0.25) = 1.5, q(0.75) = 2.5
    EXPECT_NEAR((*q)(0, 0), 1.5, 1e-12);
    EXPECT_NEAR((*q)(0, 1), 2.5, 1e-12);
    EXPECT_NEAR((*q)(1, 0), 15.0, 1e-12);
    EXPECT_NEAR((*q)(1, 1), 25.0, 1e-12);
}

TEST_P(StatsTest, QuantileBadProb)
{
    EXPECT_THROW(eval("quantile([1 2 3], -0.1);"), std::runtime_error);
    EXPECT_THROW(eval("quantile([1 2 3], 1.5);"), std::runtime_error);
}

TEST_P(StatsTest, PrctileEquivalentToQuantile)
{
    // prctile(X, 50) == quantile(X, 0.5)
    EXPECT_DOUBLE_EQ(evalScalar("prctile([1 2 3 4 5], 50);"),
                     evalScalar("quantile([1 2 3 4 5], 0.5);"));
    EXPECT_DOUBLE_EQ(evalScalar("prctile([1 2 3 4 5], 25);"),
                     evalScalar("quantile([1 2 3 4 5], 0.25);"));
}

TEST_P(StatsTest, PrctileBadProb)
{
    EXPECT_THROW(eval("prctile([1 2 3], 150);"), std::runtime_error);
    EXPECT_THROW(eval("prctile([1 2 3], -10);"), std::runtime_error);
}

// ============================================================
// mode
// ============================================================

TEST_P(StatsTest, ModeVector)
{
    // mode([1 2 2 3 3 3 4]) == 3 (most frequent, freq 3)
    EXPECT_DOUBLE_EQ(evalScalar("mode([1 2 2 3 3 3 4]);"), 3.0);
}

TEST_P(StatsTest, ModeTieBreakSmallest)
{
    // [1 1 2 2 3] — both 1 and 2 appear twice, 1 wins (smallest)
    EXPECT_DOUBLE_EQ(evalScalar("mode([1 1 2 2 3]);"), 1.0);
}

TEST_P(StatsTest, ModeAllUnique)
{
    // All unique → smallest value wins (each freq=1)
    EXPECT_DOUBLE_EQ(evalScalar("mode([5 4 3 2 1]);"), 1.0);
}

TEST_P(StatsTest, ModeWithFreqOutput)
{
    // [m, f] = mode(...) — second output is frequency.
    eval("function [a, b] = wrapMode(v)\n"
         "  [a, b] = mode(v);\n"
         "end");
    eval("[m, f] = wrapMode([1 2 2 3 3 3 4]);");
    EXPECT_DOUBLE_EQ(getVar("m"), 3.0);
    EXPECT_DOUBLE_EQ(getVar("f"), 3.0);
}

TEST_P(StatsTest, ModeMatrixDim1)
{
    // Per column. col 0 = [1,1,2,2,3], col 1 = [5,5,5,2,1]
    eval("m = mode([1 5; 1 5; 2 5; 2 2; 3 1]);");
    auto *m = getVarPtr("m");
    EXPECT_EQ(rows(*m), 1u);
    EXPECT_EQ(cols(*m), 2u);
    EXPECT_DOUBLE_EQ(m->doubleData()[0], 1.0);  // tie between 1 and 2 → 1
    EXPECT_DOUBLE_EQ(m->doubleData()[1], 5.0);  // 5 appears 3 times
}

TEST_P(StatsTest, ModeMatrixDim2)
{
    eval("m = mode([1 1 2; 5 5 5; 7 8 9], 2);");
    auto *m = getVarPtr("m");
    EXPECT_EQ(rows(*m), 3u);
    EXPECT_EQ(cols(*m), 1u);
    EXPECT_DOUBLE_EQ(m->doubleData()[0], 1.0);  // [1,1,2]
    EXPECT_DOUBLE_EQ(m->doubleData()[1], 5.0);  // [5,5,5]
    EXPECT_DOUBLE_EQ(m->doubleData()[2], 7.0);  // tie all → smallest = 7
}

// ============================================================
// 3D coverage — make sure dim handling works in all 3 axes
// ============================================================

TEST_P(StatsTest, Var3DDim1)
{
    eval("A = zeros(3, 2, 2); "
         "A(:,:,1) = [1 4; 2 5; 3 6]; "
         "A(:,:,2) = [10 40; 20 50; 30 60]; "
         "v = var(A, 0, 1);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 2u);
    // page 1 cols: [1,2,3] var=1, [4,5,6] var=1
    EXPECT_NEAR((*v)(0, 0, 0), 1.0, 1e-12);
    EXPECT_NEAR((*v)(0, 1, 0), 1.0, 1e-12);
    // page 2 cols: [10,20,30] var=100, [40,50,60] var=100
    EXPECT_NEAR((*v)(0, 0, 1), 100.0, 1e-12);
    EXPECT_NEAR((*v)(0, 1, 1), 100.0, 1e-12);
}

TEST_P(StatsTest, Var3DDim3)
{
    eval("A = zeros(2, 2, 3); "
         "A(:,:,1) = [1 2; 3 4]; "
         "A(:,:,2) = [5 6; 7 8]; "
         "A(:,:,3) = [9 10; 11 12]; "
         "v = var(A, 0, 3);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    // (0,0): [1,5,9] var = 16
    EXPECT_NEAR((*v)(0, 0), 16.0, 1e-12);
    EXPECT_NEAR((*v)(1, 0), 16.0, 1e-12);
    EXPECT_NEAR((*v)(0, 1), 16.0, 1e-12);
    EXPECT_NEAR((*v)(1, 1), 16.0, 1e-12);
}

TEST_P(StatsTest, Std3DDim2)
{
    eval("A = zeros(2, 3, 2); "
         "A(:,:,1) = [1 2 3; 4 5 6]; "
         "A(:,:,2) = [10 20 30; 40 50 60]; "
         "s = std(A, 0, 2);");
    auto *s = getVarPtr("s");
    EXPECT_EQ(rows(*s), 2u);
    EXPECT_EQ(cols(*s), 1u);
    // page 1 rows: [1,2,3] std=1, [4,5,6] std=1
    EXPECT_NEAR((*s)(0, 0, 0), 1.0, 1e-12);
    EXPECT_NEAR((*s)(1, 0, 0), 1.0, 1e-12);
    // page 2 rows: [10,20,30] std=10, [40,50,60] std=10
    EXPECT_NEAR((*s)(0, 0, 1), 10.0, 1e-12);
    EXPECT_NEAR((*s)(1, 0, 1), 10.0, 1e-12);
}

INSTANTIATE_DUAL(StatsTest);
