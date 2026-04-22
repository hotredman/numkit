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

// ============================================================
// dim overloads for existing reductions
// (sum, mean, prod, min, max, cumsum)
// ============================================================
//
// The original implementations auto-detect the reduction axis (first
// non-singleton). The three-arg form takes an explicit dim and routes
// through applyAlongDim. These tests pin both shape and value behavior.

class ReductionDimTest : public DualEngineTest
{};

// ── sum ─────────────────────────────────────────────────────

TEST_P(ReductionDimTest, SumMatrixDim1)
{
    eval("v = sum([1 2 3; 4 5 6; 7 8 9]);");  // default = dim=1
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 12.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 15.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 18.0);
}

TEST_P(ReductionDimTest, SumMatrixExplicitDim1)
{
    eval("v = sum([1 2 3; 4 5 6; 7 8 9], 1);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 12.0);
}

TEST_P(ReductionDimTest, SumMatrixDim2)
{
    eval("v = sum([1 2 3; 4 5 6; 7 8 9], 2);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 1u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 6.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 15.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 24.0);
}

TEST_P(ReductionDimTest, Sum3DDim3)
{
    eval("A = zeros(2, 2, 3); "
         "A(:,:,1) = [1 2; 3 4]; "
         "A(:,:,2) = [5 6; 7 8]; "
         "A(:,:,3) = [9 10; 11 12]; "
         "v = sum(A, 3);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_DOUBLE_EQ((*v)(0, 0), 15.0);  // 1+5+9
    EXPECT_DOUBLE_EQ((*v)(1, 0), 21.0);  // 3+7+11
    EXPECT_DOUBLE_EQ((*v)(0, 1), 18.0);
    EXPECT_DOUBLE_EQ((*v)(1, 1), 24.0);
}

TEST_P(ReductionDimTest, SumNoDimAutoDetectMatchesDim1)
{
    // sum(M) and sum(M, 1) must agree for 2D.
    EXPECT_DOUBLE_EQ(evalScalar("a = sum([1 2 3; 4 5 6]); a(1)"),
                     evalScalar("b = sum([1 2 3; 4 5 6], 1); b(1)"));
}

// ── mean ────────────────────────────────────────────────────

TEST_P(ReductionDimTest, MeanMatrixDim2)
{
    eval("m = mean([1 2 3; 4 5 6], 2);");
    auto *m = getVarPtr("m");
    EXPECT_EQ(rows(*m), 2u);
    EXPECT_EQ(cols(*m), 1u);
    EXPECT_DOUBLE_EQ(m->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(m->doubleData()[1], 5.0);
}

// ── prod ────────────────────────────────────────────────────

TEST_P(ReductionDimTest, ProdMatrixDim1)
{
    eval("p = prod([1 2; 3 4; 5 6], 1);");
    auto *p = getVarPtr("p");
    EXPECT_EQ(rows(*p), 1u);
    EXPECT_EQ(cols(*p), 2u);
    EXPECT_DOUBLE_EQ(p->doubleData()[0], 15.0);  // 1*3*5
    EXPECT_DOUBLE_EQ(p->doubleData()[1], 48.0);  // 2*4*6
}

TEST_P(ReductionDimTest, ProdMatrixDim2)
{
    eval("p = prod([1 2 3; 4 5 6], 2);");
    auto *p = getVarPtr("p");
    EXPECT_EQ(rows(*p), 2u);
    EXPECT_EQ(cols(*p), 1u);
    EXPECT_DOUBLE_EQ(p->doubleData()[0], 6.0);  // 1*2*3
    EXPECT_DOUBLE_EQ(p->doubleData()[1], 120.0); // 4*5*6
}

// ── max / min with dim ─────────────────────────────────────

TEST_P(ReductionDimTest, MaxMatrixDim2)
{
    // max(M, [], 2) — reduce along rows. [] placeholder is empty.
    eval("v = max([1 2 3; 6 5 4; 7 9 8], [], 2);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 1u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 6.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 9.0);
}

TEST_P(ReductionDimTest, MaxMatrixDim2WithIndex)
{
    eval("function [a, b] = wrap(M)\n"
         "  [a, b] = max(M, [], 2);\n"
         "end");
    eval("[v, i] = wrap([1 2 3; 6 5 4; 7 9 8]);");
    auto *v = getVarPtr("v");
    auto *idx = getVarPtr("i");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 6.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 9.0);
    EXPECT_DOUBLE_EQ(idx->doubleData()[0], 3.0);  // 1-based
    EXPECT_DOUBLE_EQ(idx->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(idx->doubleData()[2], 2.0);
}

TEST_P(ReductionDimTest, MinMatrixDim1)
{
    eval("v = min([3 1 4; 1 5 9; 2 6 5], [], 1);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 4.0);
}

TEST_P(ReductionDimTest, MaxElementwiseStillWorks)
{
    // Make sure adding the dim form didn't break elementwise max(A,B).
    eval("v = max([1 5 3], [4 2 6]);");
    auto *v = getVarPtr("v");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 6.0);
}

// ── cumsum ──────────────────────────────────────────────────

TEST_P(ReductionDimTest, CumsumMatrixDim1)
{
    eval("c = cumsum([1 2 3; 4 5 6; 7 8 9], 1);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(rows(*c), 3u);
    EXPECT_EQ(cols(*c), 3u);
    EXPECT_DOUBLE_EQ((*c)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*c)(1, 0), 5.0);
    EXPECT_DOUBLE_EQ((*c)(2, 0), 12.0);
    EXPECT_DOUBLE_EQ((*c)(2, 2), 18.0);  // 3+6+9
}

TEST_P(ReductionDimTest, CumsumMatrixDim2)
{
    eval("c = cumsum([1 2 3; 4 5 6], 2);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(rows(*c), 2u);
    EXPECT_EQ(cols(*c), 3u);
    EXPECT_DOUBLE_EQ((*c)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*c)(0, 1), 3.0);
    EXPECT_DOUBLE_EQ((*c)(0, 2), 6.0);
    EXPECT_DOUBLE_EQ((*c)(1, 0), 4.0);
    EXPECT_DOUBLE_EQ((*c)(1, 1), 9.0);
    EXPECT_DOUBLE_EQ((*c)(1, 2), 15.0);
}

TEST_P(ReductionDimTest, Cumsum3DDim3)
{
    eval("A = zeros(2, 2, 3); "
         "A(:,:,1) = [1 2; 3 4]; "
         "A(:,:,2) = [5 6; 7 8]; "
         "A(:,:,3) = [9 10; 11 12]; "
         "c = cumsum(A, 3);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(rows(*c), 2u);
    EXPECT_EQ(cols(*c), 2u);
    // Page 1 == input page 1
    EXPECT_DOUBLE_EQ((*c)(0, 0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*c)(1, 1, 0), 4.0);
    // Page 2 = page1 + page2
    EXPECT_DOUBLE_EQ((*c)(0, 0, 1), 6.0);   // 1+5
    EXPECT_DOUBLE_EQ((*c)(1, 1, 1), 12.0);  // 4+8
    // Page 3 cumulative
    EXPECT_DOUBLE_EQ((*c)(0, 0, 2), 15.0);  // 1+5+9
    EXPECT_DOUBLE_EQ((*c)(1, 1, 2), 24.0);  // 4+8+12
}

INSTANTIATE_DUAL(ReductionDimTest);

// ============================================================
// NaN-aware reductions (Phase 2)
// ============================================================
//
// Semantics summary (matches MATLAB):
//   * nansum   : all-NaN slice → 0
//   * nanmean / nanmax / nanmin / nanvar / nanstd / nanmedian
//                : all-NaN slice → NaN
//   * partial NaN : ignored, computed on remaining N_valid elements
//   * nanvar/nanstd : N-1 normalisation uses N_valid (not the original N)

class NanReductionTest : public DualEngineTest
{};

// ── nansum ──────────────────────────────────────────────────

TEST_P(NanReductionTest, NansumSkipsNaN)
{
    EXPECT_DOUBLE_EQ(evalScalar("nansum([1 NaN 2 NaN 3]);"), 6.0);
}

TEST_P(NanReductionTest, NansumAllNaNReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("nansum([NaN NaN NaN]);"), 0.0);
}

TEST_P(NanReductionTest, NansumNoNaNMatchesSum)
{
    EXPECT_DOUBLE_EQ(evalScalar("nansum([1 2 3 4 5]);"),
                     evalScalar("sum([1 2 3 4 5]);"));
}

TEST_P(NanReductionTest, NansumMatrixDim2)
{
    eval("v = nansum([1 NaN 3; NaN 5 6], 2);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 1u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 4.0);   // 1+3
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 11.0);  // 5+6
}

// ── nanmean ─────────────────────────────────────────────────

TEST_P(NanReductionTest, NanmeanSkipsNaN)
{
    // Mean of [1, 2, 3, 4] (NaN filtered) = 2.5
    EXPECT_DOUBLE_EQ(evalScalar("nanmean([1 NaN 2 3 NaN 4]);"), 2.5);
}

TEST_P(NanReductionTest, NanmeanAllNaNReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("nanmean([NaN NaN]);")));
}

TEST_P(NanReductionTest, NanmeanMatrixDim1)
{
    eval("v = nanmean([1 NaN; 3 4; 5 6]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 3.0);  // mean(1,3,5)
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 5.0);  // mean(4,6)
}

// ── nanmax / nanmin ─────────────────────────────────────────

TEST_P(NanReductionTest, NanmaxSkipsNaN)
{
    EXPECT_DOUBLE_EQ(evalScalar("nanmax([NaN 3 NaN 7 1 NaN]);"), 7.0);
}

TEST_P(NanReductionTest, NanmaxAllNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("nanmax([NaN NaN]);")));
}

TEST_P(NanReductionTest, NanminSkipsNaN)
{
    EXPECT_DOUBLE_EQ(evalScalar("nanmin([NaN 3 NaN 7 1 NaN]);"), 1.0);
}

TEST_P(NanReductionTest, NanminMatrixDim2)
{
    eval("v = nanmin([5 NaN 3; NaN 1 NaN], 2);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 1u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 1.0);
}

// ── nanvar / nanstd ─────────────────────────────────────────

TEST_P(NanReductionTest, NanvarSkipsNaN)
{
    // var([1 2 3 4 5]) = 2.5 — adding NaNs in between must give the same value
    EXPECT_NEAR(evalScalar("nanvar([1 NaN 2 NaN 3 4 5]);"), 2.5, 1e-12);
}

TEST_P(NanReductionTest, NanvarPopulationFlag)
{
    // population variance of [1 2 3 4 5] = 2.0 with normFlag=1
    EXPECT_NEAR(evalScalar("nanvar([NaN 1 2 3 NaN 4 5], 1);"), 2.0, 1e-12);
}

TEST_P(NanReductionTest, NanvarSingleValidValue)
{
    // Only one non-NaN: var with N-1 normalisation is undefined → NaN.
    EXPECT_TRUE(std::isnan(evalScalar("nanvar([NaN 5 NaN]);")));
    // Population variance of one value = 0.
    EXPECT_DOUBLE_EQ(evalScalar("nanvar([NaN 5 NaN], 1);"), 0.0);
}

TEST_P(NanReductionTest, NanstdSkipsNaN)
{
    EXPECT_NEAR(evalScalar("nanstd([1 NaN 2 NaN 3 4 5]);"), std::sqrt(2.5), 1e-12);
}

// ── nanmedian ───────────────────────────────────────────────

TEST_P(NanReductionTest, NanmedianOddCount)
{
    EXPECT_DOUBLE_EQ(evalScalar("nanmedian([3 NaN 1 NaN 5 NaN 2 4]);"),
                     3.0);  // median of [1,2,3,4,5]
}

TEST_P(NanReductionTest, NanmedianEvenCount)
{
    // Non-NaN values: [1,2,3,4] → median = 2.5
    EXPECT_DOUBLE_EQ(evalScalar("nanmedian([4 NaN 1 2 NaN 3]);"), 2.5);
}

TEST_P(NanReductionTest, NanmedianAllNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("nanmedian([NaN NaN NaN]);")));
}

// ── 3D dim coverage ─────────────────────────────────────────

TEST_P(NanReductionTest, Nansum3DDim3)
{
    eval("A = zeros(2, 2, 3); "
         "A(:,:,1) = [1 2; NaN 4]; "
         "A(:,:,2) = [NaN 6; 7 NaN]; "
         "A(:,:,3) = [9 NaN; 11 12]; "
         "v = nansum(A, 3);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    // (0,0): [1, NaN, 9]    → 10
    // (1,0): [NaN, 7, 11]   → 18
    // (0,1): [2, 6, NaN]    → 8
    // (1,1): [4, NaN, 12]   → 16
    EXPECT_DOUBLE_EQ((*v)(0, 0), 10.0);
    EXPECT_DOUBLE_EQ((*v)(1, 0), 18.0);
    EXPECT_DOUBLE_EQ((*v)(0, 1), 8.0);
    EXPECT_DOUBLE_EQ((*v)(1, 1), 16.0);
}

TEST_P(NanReductionTest, NanmeanReturnsNaNForAllNaNSlice)
{
    // Column 0 has a real value, column 1 is all-NaN.
    eval("v = nanmean([1 NaN; 2 NaN; 3 NaN]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 2.0);
    EXPECT_TRUE(std::isnan(v->doubleData()[1]));
}

INSTANTIATE_DUAL(NanReductionTest);

// ============================================================
// Cumulative + logical reductions (Phase 3)
// cumprod, cummax, cummin, any, all, xor, isfinite
// ============================================================

class CumLogicalTest : public DualEngineTest
{};

// ── cumprod ─────────────────────────────────────────────────

TEST_P(CumLogicalTest, CumprodVector)
{
    eval("c = cumprod([1 2 3 4 5]);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(c->numel(), 5u);
    EXPECT_DOUBLE_EQ(c->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[2], 6.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[3], 24.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[4], 120.0);
}

TEST_P(CumLogicalTest, CumprodMatrixDim2)
{
    eval("c = cumprod([1 2 3; 4 5 6], 2);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(rows(*c), 2u);
    EXPECT_EQ(cols(*c), 3u);
    EXPECT_DOUBLE_EQ((*c)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*c)(0, 1), 2.0);   // 1*2
    EXPECT_DOUBLE_EQ((*c)(0, 2), 6.0);   // 1*2*3
    EXPECT_DOUBLE_EQ((*c)(1, 0), 4.0);
    EXPECT_DOUBLE_EQ((*c)(1, 1), 20.0);  // 4*5
    EXPECT_DOUBLE_EQ((*c)(1, 2), 120.0); // 4*5*6
}

// ── cummax / cummin ─────────────────────────────────────────

TEST_P(CumLogicalTest, CummaxVector)
{
    eval("c = cummax([3 1 4 1 5 9 2 6]);");
    auto *c = getVarPtr("c");
    EXPECT_DOUBLE_EQ(c->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[2], 4.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[3], 4.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[4], 5.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[5], 9.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[6], 9.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[7], 9.0);
}

TEST_P(CumLogicalTest, CumminVector)
{
    eval("c = cummin([5 3 7 2 8 1 4]);");
    auto *c = getVarPtr("c");
    EXPECT_DOUBLE_EQ(c->doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[3], 2.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[4], 2.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[5], 1.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[6], 1.0);
}

TEST_P(CumLogicalTest, CummaxSkipsNaN)
{
    // NaN treated as identity (default 'omitnan' since R2018a).
    eval("c = cummax([1 NaN 3 NaN 2]);");
    auto *c = getVarPtr("c");
    EXPECT_DOUBLE_EQ(c->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[1], 1.0); // 1 vs NaN → 1
    EXPECT_DOUBLE_EQ(c->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[3], 3.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[4], 3.0);
}

TEST_P(CumLogicalTest, CummaxMatrixDim1)
{
    eval("c = cummax([1 9 2; 5 3 8; 4 6 7], 1);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(rows(*c), 3u);
    EXPECT_EQ(cols(*c), 3u);
    // col 0: [1,5,5], col 1: [9,9,9], col 2: [2,8,8]
    EXPECT_DOUBLE_EQ((*c)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*c)(1, 0), 5.0);
    EXPECT_DOUBLE_EQ((*c)(2, 0), 5.0);
    EXPECT_DOUBLE_EQ((*c)(2, 1), 9.0);
    EXPECT_DOUBLE_EQ((*c)(2, 2), 8.0);
}

TEST_P(CumLogicalTest, Cumprod3DDim3)
{
    eval("A = zeros(2, 2, 3); "
         "A(:,:,1) = [1 2; 3 4]; "
         "A(:,:,2) = [2 2; 2 2]; "
         "A(:,:,3) = [3 3; 3 3]; "
         "c = cumprod(A, 3);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(rows(*c), 2u);
    EXPECT_EQ(cols(*c), 2u);
    // Page 3 = page1 * page2 * page3
    EXPECT_DOUBLE_EQ((*c)(0, 0, 2), 6.0);   // 1*2*3
    EXPECT_DOUBLE_EQ((*c)(1, 0, 2), 18.0);  // 3*2*3
    EXPECT_DOUBLE_EQ((*c)(0, 1, 2), 12.0);  // 2*2*3
    EXPECT_DOUBLE_EQ((*c)(1, 1, 2), 24.0);  // 4*2*3
}

// ── any / all ───────────────────────────────────────────────

TEST_P(CumLogicalTest, AnyVectorTrue)
{
    EXPECT_TRUE(evalBool("any([0 0 1 0]);"));
}

TEST_P(CumLogicalTest, AnyVectorFalse)
{
    EXPECT_FALSE(evalBool("any([0 0 0]);"));
}

TEST_P(CumLogicalTest, AnyEmpty)
{
    // any([]) → false (vacuous).
    eval("v = any([]);");
    auto *v = getVarPtr("v");
    EXPECT_TRUE(v->numel() == 0 || v->toBool() == false);
}

TEST_P(CumLogicalTest, AllVectorTrue)
{
    EXPECT_TRUE(evalBool("all([1 2 -3 4]);"));
}

TEST_P(CumLogicalTest, AllVectorFalse)
{
    EXPECT_FALSE(evalBool("all([1 0 1]);"));
}

TEST_P(CumLogicalTest, AnyMatrixDim2)
{
    eval("v = any([0 0 0; 0 1 0; 0 0 0], 2);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 1u);
    EXPECT_FALSE(v->logicalData()[0] != 0);
    EXPECT_TRUE (v->logicalData()[1] != 0);
    EXPECT_FALSE(v->logicalData()[2] != 0);
}

TEST_P(CumLogicalTest, AllMatrixDim1)
{
    eval("v = all([1 0 1; 1 1 0; 1 1 1]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_TRUE (v->logicalData()[0] != 0);  // col 0: [1,1,1]
    EXPECT_FALSE(v->logicalData()[1] != 0);  // col 1: [0,1,1]
    EXPECT_FALSE(v->logicalData()[2] != 0);  // col 2: [1,0,1]
}

TEST_P(CumLogicalTest, AnyTreatsNaNAsTrue)
{
    // MATLAB: any(NaN) → true (NaN != 0).
    EXPECT_TRUE(evalBool("any([0 NaN 0]);"));
}

// ── xor ─────────────────────────────────────────────────────

TEST_P(CumLogicalTest, XorBasic)
{
    EXPECT_TRUE(evalBool("xor(1, 0);"));
    EXPECT_TRUE(evalBool("xor(0, 1);"));
    EXPECT_FALSE(evalBool("xor(1, 1);"));
    EXPECT_FALSE(evalBool("xor(0, 0);"));
}

TEST_P(CumLogicalTest, XorElementwise)
{
    eval("v = xor([1 0 1 0], [1 1 0 0]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 4u);
    EXPECT_FALSE(v->logicalData()[0] != 0); // 1^1
    EXPECT_TRUE (v->logicalData()[1] != 0); // 0^1
    EXPECT_TRUE (v->logicalData()[2] != 0); // 1^0
    EXPECT_FALSE(v->logicalData()[3] != 0); // 0^0
}

TEST_P(CumLogicalTest, XorTreatsNonZeroAsTrue)
{
    // 5 and -3 both truthy → xor = false. 5 and 0 → true.
    EXPECT_FALSE(evalBool("xor(5, -3);"));
    EXPECT_TRUE (evalBool("xor(5, 0);"));
}

// ── isfinite ────────────────────────────────────────────────

TEST_P(CumLogicalTest, IsfiniteScalar)
{
    EXPECT_TRUE (evalBool("isfinite(3.14);"));
    EXPECT_FALSE(evalBool("isfinite(NaN);"));
    EXPECT_FALSE(evalBool("isfinite(Inf);"));
    EXPECT_FALSE(evalBool("isfinite(-Inf);"));
}

TEST_P(CumLogicalTest, IsfiniteVector)
{
    eval("v = isfinite([1 NaN 2 Inf 3 -Inf]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 6u);
    EXPECT_TRUE (v->logicalData()[0] != 0);
    EXPECT_FALSE(v->logicalData()[1] != 0);
    EXPECT_TRUE (v->logicalData()[2] != 0);
    EXPECT_FALSE(v->logicalData()[3] != 0);
    EXPECT_TRUE (v->logicalData()[4] != 0);
    EXPECT_FALSE(v->logicalData()[5] != 0);
}

INSTANTIATE_DUAL(CumLogicalTest);
