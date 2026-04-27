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

#include <cmath>

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

// ── Phase P5 SIMD two-pass var/std coverage ─────────────────
//
// The existing StatsTest cases work on tiny vectors that don't
// exercise the 4× unrolled SIMD body of varianceTwoPass. These tests
// hit N >> 4*lanes (Highway: 4 doubles on AVX2, 8 on AVX-512) so the
// body, partial-vector tail, and scalar-final-tail paths all execute.

TEST_P(StatsTest, VarLargeNMatchesReference)
{
    // Build a deterministic sequence: x = 1..1000. Reference variance
    // for [1..N] (sample form, /N-1) = N*(N+1)/12.
    eval("x = 1:1000; v = var(x);");
    auto *v = getVarPtr("v");
    const double ref = 1000.0 * 1001.0 / 12.0;     // = 83416.6666...
    EXPECT_NEAR(v->toScalar(), ref, 1e-9 * std::abs(ref));
}

TEST_P(StatsTest, VarLargeNRandomMatchesScalarReference)
{
    // Compare two-pass output against an explicit sum-of-squared-
    // deviations computed by the script itself. Catches drift between
    // the SIMD kernel and the scalar reference at N=4096 (well past
    // any unroll boundary).
    eval("rng(7); x = randn(4096, 1); "
         "m = mean(x); ref = sum((x - m).^2) / (numel(x) - 1); "
         "v = var(x); err = abs(v - ref) / abs(ref);");
    auto *err = getVarPtr("err");
    EXPECT_LT(err->toScalar(), 1e-12);
}

TEST_P(StatsTest, StdMatchesSqrtVar)
{
    eval("rng(11); x = randn(2048, 1); "
         "delta = abs(std(x) - sqrt(var(x)));");
    auto *delta = getVarPtr("delta");
    EXPECT_LT(delta->toScalar(), 1e-12);
}

// SIMD tail handling — N just past a multiple of typical lane counts.
// Covers cases where i + 4*N <= n is false but i + N <= n is true
// (1× SIMD body), and where i < n is the only remainder (scalar tail).
// ── 2D / 3D matrix-dim coverage that exercises the SIMD body ─
//
// Existing VarMatrixDim*/StdMatrixDim*/Var3DDim* use 3-row matrices;
// the per-slice SIMD scan body needs slice length >= 4 (AVX2) or >= 8
// (AVX-512) before the unrolled body fires. These cases use 64-row
// matrices so the body, partial-vector, and scalar-tail all execute
// per slice.

TEST_P(StatsTest, VarMatrixDim1LargeRowsPerColumnReference)
{
    // 64-row, 3-col matrix. var(M) reduces along columns (dim=1).
    // Column k = (1..64) + 100*k; var of arithmetic progression
    // 1..64 = N*(N+1)/12 where N=64, so reference = 64*65/12.
    eval("M = repmat((1:64)', 1, 3) + 100 * repmat(0:2, 64, 1); v = var(M);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 3u);
    const double ref = 64.0 * 65.0 / 12.0;
    for (size_t c = 0; c < 3; ++c)
        EXPECT_NEAR(v->doubleData()[c], ref, 1e-9 * std::abs(ref))
            << "at col " << c;
}

TEST_P(StatsTest, StdMatrixDim1LargeRows)
{
    eval("M = repmat((1:64)', 1, 3); s = std(M);");
    auto *s = getVarPtr("s");
    const double ref = std::sqrt(64.0 * 65.0 / 12.0);
    for (size_t c = 0; c < 3; ++c)
        EXPECT_NEAR(s->doubleData()[c], ref, 1e-9 * std::abs(ref));
}

TEST_P(StatsTest, Var3DDim1LargeRowsPerColumnPerPage)
{
    // 64×3×2: var along dim=1 → 1×3×2 result. Each column-page slice
    // is a 64-element column → SIMD body fires.
    eval("R = 64; C = 3; P = 2; "
         "M = zeros(R, C, P); "
         "for p = 1:P, for c = 1:C, M(:,c,p) = (1:R)' + 1000*p; end; end; "
         "v = var(M);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_EQ(v->dims().pages(), 2u);
    const double ref = 64.0 * 65.0 / 12.0;
    for (size_t p = 0; p < 2; ++p)
        for (size_t c = 0; c < 3; ++c)
            EXPECT_NEAR((*v)(0, c, p), ref, 1e-9 * std::abs(ref))
                << "at p=" << p << " c=" << c;
}

TEST_P(StatsTest, VarOddSizesAroundSimdLanes)
{
    for (int n : {17, 33, 65, 127, 257}) {
        const std::string code =
            "x = 1:" + std::to_string(n) + "; v = var(x);";
        eval(code);
        auto *v = getVarPtr("v");
        const double N = static_cast<double>(n);
        const double ref = N * (N + 1.0) / 12.0;
        EXPECT_NEAR(v->toScalar(), ref, 1e-9 * std::abs(ref))
            << "at n=" << n;
    }
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

TEST_P(ReductionDimTest, SumMatrixDim2LargeColumnPass)
{
    // 64-row × 4-col matrix. dim=2 routes through the column-pass
    // SIMD addInto kernel — slice = 4 columns, totals = 64 rows
    // (one full SIMD body per column iter on AVX2).
    // Element M(r, c) = (r + 1) + 100 * (c - 1);
    //   row r totals = sum_{c=0..3} ((r+1) + 100*c) = 4*(r+1) + 100*0+100*1+100*2+100*3
    //                 = 4*(r+1) + 600.
    eval("R = 64; C = 4; "
         "M = repmat((1:R)', 1, C) + 100 * repmat(0:C-1, R, 1); "
         "v = sum(M, 2);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 64u);
    EXPECT_EQ(cols(*v), 1u);
    for (size_t r = 0; r < 64; ++r) {
        const double expected = 4.0 * static_cast<double>(r + 1) + 600.0;
        EXPECT_DOUBLE_EQ(v->doubleData()[r], expected) << "at row " << r;
    }
}

TEST_P(ReductionDimTest, SumMatrixDim2OddSizesAroundSimdLanes)
{
    // R = 17/33/65/127 to exercise body + partial-vector + scalar tail
    // on the addInto column-pass kernel.
    for (int r : {17, 33, 65, 127}) {
        const std::string rs = std::to_string(r);
        eval("R = " + rs + "; C = 5; "
             "M = repmat((1:R)', 1, C); "
             "v = sum(M, 2);");
        auto *v = getVarPtr("v");
        ASSERT_EQ(rows(*v), static_cast<size_t>(r));
        for (size_t i = 0; i < static_cast<size_t>(r); ++i) {
            const double expected = 5.0 * static_cast<double>(i + 1);
            EXPECT_DOUBLE_EQ(v->doubleData()[i], expected)
                << "at R=" << r << " row=" << i;
        }
    }
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

// ── ND reductions (Phase 3b) ────────────────────────────────────

TEST_P(ReductionDimTest, Sum4DAlongDim1)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); s = sum(A, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(s);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(s, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(s, 2);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(s, 3);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(s, 4);"), 5.0);
    // s(1,1,1,1) = sum A(:,1,1,1) = 1+2 = 3
    EXPECT_DOUBLE_EQ(evalScalar("s(1, 1, 1, 1);"), 3.0);
    // s(1,3,4,5) = sum A(:,3,4,5) = 119+120 = 239
    EXPECT_DOUBLE_EQ(evalScalar("s(1, 3, 4, 5);"), 239.0);
}

TEST_P(ReductionDimTest, Sum4DAlongDim4)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); s = sum(A, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(s);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(s, 4);"), 1.0);
    // s(1,1,1,1) = sum_l A(1,1,1,l) = 1 + 25 + 49 + 73 + 97 = 245
    EXPECT_DOUBLE_EQ(evalScalar("s(1, 1, 1, 1);"), 245.0);
    // sum-over-all check
    EXPECT_DOUBLE_EQ(evalScalar("sum(s(:));"), evalScalar("sum(1:120);"));
}

TEST_P(ReductionDimTest, Mean5DAlongDim3)
{
    eval("A = reshape(1:720, [2, 3, 4, 5, 6]); m = mean(A, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 3);"), 1.0);
    // m(1,1,1,1,1) = mean A(1,1,:,1,1) = mean of [1, 7, 13, 19] = 10
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1, 1, 1, 1);"), 10.0);
}

TEST_P(ReductionDimTest, Max5DAlongDim4WithIndex)
{
    eval("A = reshape(1:720, [2, 3, 4, 5, 6]); [v, i] = max(A, [], 4);");
    EXPECT_DOUBLE_EQ(evalScalar("size(v, 4);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(i, 4);"), 1.0);
    // max along axis 4 of stride-24 entries — last (largest) is at l=5.
    // v(1,1,1,1,1) = max A(1,1,1,1,1..5) = max(1, 25, 49, 73, 97) = 97
    EXPECT_DOUBLE_EQ(evalScalar("v(1, 1, 1, 1, 1);"), 97.0);
    EXPECT_DOUBLE_EQ(evalScalar("i(1, 1, 1, 1, 1);"), 5.0);
}

TEST_P(ReductionDimTest, Cumsum4DAlongDim2)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); c = cumsum(A, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(c);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(c, 2);"), 3.0);
    // First column unchanged
    EXPECT_DOUBLE_EQ(evalScalar("c(1, 1, 1, 1);"), 1.0);
    // c(1,2,1,1) = A(1,1,1,1) + A(1,2,1,1) = 1 + 3 = 4
    EXPECT_DOUBLE_EQ(evalScalar("c(1, 2, 1, 1);"), 4.0);
    // c(1,3,1,1) = 1 + 3 + 5 = 9
    EXPECT_DOUBLE_EQ(evalScalar("c(1, 3, 1, 1);"), 9.0);
}

TEST_P(ReductionDimTest, Cumprod4DAlongDim4)
{
    // cumprod along the last axis (stride-large axis). Build [2 4 8 ...]
    // along axis 4 by reshaping powers-of-2 and indexing.
    eval("A = reshape(1:36, [2, 3, 2, 3]); c = cumprod(A, 4);");
    // First axis-4 plane unchanged
    EXPECT_DOUBLE_EQ(evalScalar("c(1, 1, 1, 1);"), 1.0);
    // c(1,1,1,2) = A(1,1,1,1) * A(1,1,1,2) = 1 * 13 = 13
    EXPECT_DOUBLE_EQ(evalScalar("c(1, 1, 1, 2);"), 13.0);
    // c(1,1,1,3) = 1 * 13 * 25 = 325
    EXPECT_DOUBLE_EQ(evalScalar("c(1, 1, 1, 3);"), 325.0);
}

TEST_P(ReductionDimTest, Any4DAlongDim3)
{
    eval("A = zeros(2, 3, 4, 2); A(1, 1, 2, 1) = 1; B = any(A, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(B);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 2, 1, 1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 2);"), 0.0);
}

TEST_P(ReductionDimTest, All4DAlongDim4)
{
    eval("A = ones(2, 3, 2, 5); A(1, 1, 1, 3) = 0; B = all(A, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B, 4);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1, 1, 1, 1);"), 0.0); // one zero in slice
    EXPECT_DOUBLE_EQ(evalScalar("B(2, 1, 1, 1);"), 1.0); // all ones
}

TEST_P(ReductionDimTest, Var4DAlongDim2)
{
    // var along dim 2: each (1, :, k, l) slice's variance
    eval("A = reshape(1:120, [2, 3, 4, 5]); v = var(A, 0, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(v);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(v, 2);"), 1.0);
    // var of [1, 3, 5] (n=3, normFlag=0 → /n-1 = 2) = ((1-3)^2+(3-3)^2+(5-3)^2)/2 = 8/2 = 4
    EXPECT_DOUBLE_EQ(evalScalar("v(1, 1, 1, 1);"), 4.0);
}

TEST_P(ReductionDimTest, Nansum4DAlongDim3)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]);"
         "A(1, 1, 2, 1) = NaN;"
         "s = nansum(A, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(s);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(s, 3);"), 1.0);
    // Slice A(1,1,:,1) = [1, 7, 13, 19] → with A(1,1,2,1)=NaN nansum = 1+13+19 = 33
    EXPECT_DOUBLE_EQ(evalScalar("s(1, 1, 1, 1);"), 33.0);
    // Untouched slice
    EXPECT_DOUBLE_EQ(evalScalar("s(2, 1, 1, 1);"), 2.0 + 8.0 + 14.0 + 20.0);
}

// ── Phase A audit: reductions assumed "free via applyAlongDim" ───
// These exercise paths that should work without explicit ND code in
// the per-op function. Failures here mean a missing dispatcher branch.

TEST_P(ReductionDimTest, Min4DAlongDim2WithIndex)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); [v, i] = min(A, [], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("size(v, 2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(i, 2);"), 1.0);
    // min along dim 2 of A(1,:,1,1) = [1, 3, 5] → v=1, i=1
    EXPECT_DOUBLE_EQ(evalScalar("v(1, 1, 1, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("i(1, 1, 1, 1);"), 1.0);
}

TEST_P(ReductionDimTest, Prod4DAlongDim1)
{
    // 2-element products along dim 1 of A(1:2,j,k,l) for reshape(1:120,[2,3,4,5])
    eval("A = reshape(1:120, [2, 3, 4, 5]); p = prod(A, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("size(p, 1);"), 1.0);
    // p(1,1,1,1) = A(1,1,1,1)*A(2,1,1,1) = 1*2 = 2
    EXPECT_DOUBLE_EQ(evalScalar("p(1, 1, 1, 1);"), 2.0);
    // p(1,3,4,5) = 119*120 = 14280
    EXPECT_DOUBLE_EQ(evalScalar("p(1, 3, 4, 5);"), 14280.0);
}

TEST_P(ReductionDimTest, Std4DAlongDim2)
{
    // Slice A(1,:,1,1) = [1, 3, 5]; sample std (normFlag=0, /n-1) = sqrt(4) = 2
    eval("A = reshape(1:120, [2, 3, 4, 5]); s = std(A, 0, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("size(s, 2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(1, 1, 1, 1);"), 2.0);
}

TEST_P(ReductionDimTest, Median4DAlongDim2)
{
    // median of [1,3,5] = 3
    eval("A = reshape(1:120, [2, 3, 4, 5]); m = median(A, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1, 1, 1);"), 3.0);
}

TEST_P(ReductionDimTest, Mode4DAlongDim2)
{
    // Each slice along dim 2: [a, b, c]. With reshape(1:120,...) all distinct
    // → mode picks smallest. Slice A(1,:,1,1)=[1,3,5] → mode=1.
    eval("A = reshape(1:120, [2, 3, 4, 5]); m = mode(A, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1, 1, 1);"), 1.0);
}

// ── nan* family ND ──

TEST_P(ReductionDimTest, Nanmean4DAlongDim3)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); A(1,1,2,1) = NaN; m = nanmean(A, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 3);"), 1.0);
    // A(1,1,:,1) with NaN = [1, NaN, 13, 19] → nanmean = 33/3 = 11
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1, 1, 1);"), 11.0);
}

TEST_P(ReductionDimTest, Nanvar4DAlongDim3)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); A(1,1,2,1) = NaN; v = nanvar(A, 0, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(v, 3);"), 1.0);
    // [1, 13, 19] mean=11, variance (n-1) = ((1-11)^2 + (13-11)^2 + (19-11)^2)/2
    // = (100 + 4 + 64) / 2 = 84
    EXPECT_DOUBLE_EQ(evalScalar("v(1, 1, 1, 1);"), 84.0);
}

TEST_P(ReductionDimTest, Nanstd4DAlongDim3)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); A(1,1,2,1) = NaN; s = nanstd(A, 0, 3);");
    EXPECT_NEAR(evalScalar("s(1, 1, 1, 1);"), std::sqrt(84.0), 1e-12);
}

TEST_P(ReductionDimTest, Nanmax4DAlongDim3)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); A(2,3,4,5) = NaN; m = nanmax(A, [], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 3);"), 1.0);
    // A(2,3,:,5) = [102, 108, 114, 120]; with NaN at A(2,3,4,5) → max of [102,108,114] = 114
    EXPECT_DOUBLE_EQ(evalScalar("m(2, 3, 1, 5);"), 114.0);
}

TEST_P(ReductionDimTest, Nanmin4DAlongDim3)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); A(1,1,1,1) = NaN; m = nanmin(A, [], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 3);"), 1.0);
    // A(1,1,:,1) = [NaN, 7, 13, 19] → nanmin = 7
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1, 1, 1);"), 7.0);
}

TEST_P(ReductionDimTest, Nanmedian4DAlongDim3)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); A(1,1,2,1) = NaN; m = nanmedian(A, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 3);"), 1.0);
    // [1, NaN, 13, 19] → nanmedian over [1, 13, 19] = 13
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1, 1, 1);"), 13.0);
}

// ── Phase 4 (round-3): min/max preserve source type ─────────────
//
// MATLAB rule: min/max default to 'native' mode, returning the same
// element type as the input. Only the index output is always DOUBLE.

TEST_P(ReductionDimTest, MaxInt32VectorReturnsInt32)
{
    eval("v = int32([5 1 9 2]); m = max(v);");
    EXPECT_DOUBLE_EQ(evalScalar("m;"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(m);"), 1.0);
}

TEST_P(ReductionDimTest, MinInt8MatrixWithIndex)
{
    eval("M = int8([3 7 2; 5 1 9]); [v, i] = min(M);");
    // Per-column min: [3, 1, 2], indices [1, 2, 1]
    EXPECT_DOUBLE_EQ(evalScalar("v(1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(v);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("i(1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("i(2);"), 2.0);
}

TEST_P(ReductionDimTest, MaxUint8MatrixDim2)
{
    eval("M = uint8([3 7 2; 5 1 9]); m = max(M, [], 2);");
    // Per-row max: [7; 9]
    EXPECT_DOUBLE_EQ(evalScalar("m(1);"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2);"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(m);"), 1.0);
}

TEST_P(ReductionDimTest, MinSingleVector)
{
    eval("v = single([3.5 1.5 2.0]); m = min(v);");
    EXPECT_DOUBLE_EQ(evalScalar("m;"), 1.5);
    EXPECT_DOUBLE_EQ(evalScalar("issingle(m);"), 1.0);
}

TEST_P(ReductionDimTest, MaxInt64Reduction)
{
    // int64 reduction must use the typed kernel (not double-promoted),
    // so values past the 53-bit double mantissa remain bit-exact.
    eval("v = int64([100; 200; 50]); m = max(v);");
    EXPECT_DOUBLE_EQ(evalScalar("m;"), 200.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(m);"), 1.0);
    // Round-trip a large value through int64 reduction: subtract from
    // result and verify exactness.
    eval("big = int64(2) ^ int64(60);"          // 1152921504606846976
         "u = int64([1; big; 5]);"
         "mb = max(u);"
         "diff = mb - big;");
    EXPECT_DOUBLE_EQ(evalScalar("double(diff);"), 0.0);
}

TEST_P(ReductionDimTest, MinMax4DInt16AlongDim4)
{
    eval("A = int16(reshape(1:24, [2, 3, 2, 2])); [v, i] = max(A, [], 4);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(v);"),     4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(v, 4);"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(v);"), 1.0);
    // v(1,1,1,1) = max(A(1,1,1,1), A(1,1,1,2)) = max(1, 13) = 13
    EXPECT_DOUBLE_EQ(evalScalar("v(1, 1, 1, 1);"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("i(1, 1, 1, 1);"), 2.0);
}

TEST_P(ReductionDimTest, MaxLogicalReturnsLogical)
{
    eval("v = logical([0 1 0 0]); m = max(v);");
    EXPECT_DOUBLE_EQ(evalScalar("m;"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(m);"), 1.0);
}

TEST_P(ReductionDimTest, MaxCharReturnsChar)
{
    eval("s = 'banana'; m = max(s);");
    // ASCII: 'a'=97, 'b'=98, 'n'=110 → max = 'n'
    EXPECT_DOUBLE_EQ(evalScalar("ischar(m);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(m);"), 110.0);
}

TEST_P(ReductionDimTest, MaxComplexThrows)
{
    eval("v = [1+2i, 3+4i, 5+0i];");
    EXPECT_THROW(eval("m = max(v);"), std::exception);
}

TEST_P(ReductionDimTest, BinaryMinInt32SameClass)
{
    eval("a = int32([3 7 2]); b = int32([5 1 9]); m = min(a, b);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(m);"), 1.0);
}

TEST_P(ReductionDimTest, BinaryMaxIntDoubleReturnsInt)
{
    // MATLAB: int wins over double in mixed arithmetic.
    eval("a = int32([3 7 2]); b = [5 1 9]; m = max(a, b);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2);"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3);"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("isinteger(m);"), 1.0);
}

TEST_P(ReductionDimTest, BinaryMinSingleDoubleReturnsSingle)
{
    eval("a = single([3 7 2]); b = [5 1 9]; m = min(a, b);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("issingle(m);"), 1.0);
}

TEST_P(ReductionDimTest, BinaryMinMixedIntClassThrows)
{
    eval("a = int32([1 2]); b = int16([3 4]);");
    EXPECT_THROW(eval("m = min(a, b);"), std::exception);
}

// ── ND empty-input shape preservation (rank ≥ 4) ────────────────
//
// MATLAB collapses the reduced dim to 1 and preserves all other dims.
// For zeros([2, 0, 3, 2]), reducing dim 2 (the empty axis) gives
// 2×1×3×2 of zeros — the slice is empty so sum of empty slice = 0.
// 2D/3D empty inputs intentionally still collapse to 0×0 (legacy).

TEST_P(ReductionDimTest, Sum4DEmptyAlongEmptyDimPreservesShape)
{
    eval("A = zeros([2, 0, 3, 2]); m = sum(A, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(m);"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 2);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 3);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 4);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(m);"), 12.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1, 1, 1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2, 1, 3, 2);"), 0.0);
}

TEST_P(ReductionDimTest, Sum4DEmptyAlongNonEmptyDimStaysEmpty)
{
    // Reducing a non-empty axis of an empty array preserves emptiness.
    // zeros([2,0,3,2]) sum dim=1 → 1×0×3×2, numel still 0.
    eval("A = zeros([2, 0, 3, 2]); m = sum(A, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(m);"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 2);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 3);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 4);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(m);"), 0.0);
}

TEST_P(ReductionDimTest, Mean5DEmptyAlongEmptyDim)
{
    // mean of empty slice = NaN (matches MATLAB).
    eval("A = zeros([2, 3, 0, 4, 2]); m = mean(A, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(m);"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 3);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(m);"),  48.0);
    // mean of 0 elements is NaN
    eval("v = m(1, 1, 1, 1, 1);");
    EXPECT_TRUE(std::isnan(evalScalar("v;")));
}

// ── ND reductions on non-DOUBLE inputs ───────────────────────────
//
// MATLAB sum/mean/etc. on integer/logical ND inputs return DOUBLE
// (since the cast int → accumulator → double is the natural path
// for sum). We follow that convention: non-DOUBLE input is read via
// elemAsDouble inside the reduction kernel; output is DOUBLE.

TEST_P(ReductionDimTest, Sum4DInt32AlongDim2)
{
    eval("A = int32(reshape(1:24, [2, 3, 2, 2])); m = sum(A, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(m);"),    4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 2);"),  1.0);
    // m(1,1,1,1) = A(1,1,1,1)+A(1,2,1,1)+A(1,3,1,1) = 1+3+5 = 9
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1, 1, 1);"), 9.0);
    // m(2,1,2,2) = A(2,1,2,2)+A(2,2,2,2)+A(2,3,2,2) = 20+22+24 = 66
    EXPECT_DOUBLE_EQ(evalScalar("m(2, 1, 2, 2);"), 66.0);
}

TEST_P(ReductionDimTest, Mean4DLogicalAlongDim4)
{
    // logical() on 4D input now preserves rank (was capping at 3D).
    eval("A = logical(reshape(mod(1:24, 2), [2, 3, 2, 2])); m = mean(A, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"),   4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 4);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ndims(m);"),   4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 4);"), 1.0);
    // mean of two 0/1 values along axis 4 must be in [0, 1]
    double v = evalScalar("m(1, 1, 1, 1);");
    EXPECT_TRUE(v >= 0.0 && v <= 1.0);
}

TEST_P(ReductionDimTest, Sum4DUint8AlongDim4)
{
    eval("A = uint8(reshape(1:24, [2, 3, 2, 2])); m = sum(A, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(m);"),   4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 4);"), 1.0);
    // m(1,1,1,1) = A(1,1,1,1) + A(1,1,1,2) = 1 + 13 = 14
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1, 1, 1);"), 14.0);
    // m(2,3,2,1) = A(2,3,2,1) + A(2,3,2,2) = 12 + 24 = 36
    EXPECT_DOUBLE_EQ(evalScalar("m(2, 3, 2, 1);"), 36.0);
}

// ── 1D/2D/3D reductions on non-DOUBLE inputs (round-2 bonus) ────
//
// Same lift as the ND case: vector path + forEachSlice (2D/3D) +
// reduce() now read non-DOUBLE input via elemAsDouble. Output stays
// DOUBLE per MATLAB convention.

TEST_P(ReductionDimTest, SumInt32VectorReturnsScalar)
{
    eval("v = int32([1 2 3 4 5]); s = sum(v);");
    EXPECT_DOUBLE_EQ(evalScalar("s;"), 15.0);
}

TEST_P(ReductionDimTest, MeanUint8MatrixDim1)
{
    eval("M = uint8(reshape(1:12, [3, 4])); m = mean(M, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 2);"), 4.0);
    // mean of column 1 = (1+2+3)/3 = 2
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1);"), 2.0);
    // mean of column 4 = (10+11+12)/3 = 11
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 4);"), 11.0);
}

TEST_P(ReductionDimTest, ProdInt16MatrixDim2)
{
    eval("M = int16(reshape(1:6, [2, 3])); p = prod(M, 2);");
    // M = [1 3 5; 2 4 6]; prod row 1 = 15, row 2 = 48
    EXPECT_DOUBLE_EQ(evalScalar("p(1);"), 15.0);
    EXPECT_DOUBLE_EQ(evalScalar("p(2);"), 48.0);
}

TEST_P(ReductionDimTest, SumSingle3DAlongDim3)
{
    eval("A = single(reshape(1:24, [2, 3, 4])); s = sum(A, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(s, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(s, 2);"), 3.0);
    // s(1,1) = A(1,1,1) + A(1,1,2) + A(1,1,3) + A(1,1,4) = 1 + 7 + 13 + 19 = 40
    EXPECT_DOUBLE_EQ(evalScalar("s(1, 1);"), 40.0);
}

TEST_P(ReductionDimTest, SumLogicalMatrixDim2)
{
    eval("M = logical([1 0 1; 0 1 1]); s = sum(M, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("s(1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(2);"), 2.0);
}

TEST_P(ReductionDimTest, Sum4DSingleAlongDim3)
{
    eval("A = single(reshape(1:48, [2, 3, 4, 2])); m = sum(A, 3);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(m);"),   4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m, 3);"), 1.0);
    // m(1,1,1,1) = sum over k of A(1,1,k,1), k=1..4
    // A(1,1,1,1)=1, A(1,1,2,1)=7, A(1,1,3,1)=13, A(1,1,4,1)=19 → 40
    EXPECT_DOUBLE_EQ(evalScalar("m(1, 1, 1, 1);"), 40.0);
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

// ── Phase P2 SIMD single-pass coverage ───────────────────────
//
// The SIMD nansum/nanmean kernels read 4× lanes per iteration. Existing
// tests work on tiny vectors that don't cross those unroll boundaries.
// The cases below pin: large N + scattered NaN, NaN at SIMD-block
// boundaries, and the parallel-count accumulator's correctness under
// dense NaN.

TEST_P(NanReductionTest, NansumLargeNScatteredNaN)
{
    // 1000 elements with NaN injected at known positions via for-loop
    // (avoids logical-indexed assignment which isn't a builtin yet).
    // Every 7th index (1-based: 1, 8, 15, ..., 995) becomes NaN.
    eval("x = (1:1000)'; for i = 1:7:1000, x(i) = NaN; end; "
         "ref = 0; for i = 1:1000, if ~isnan(x(i)), ref = ref + x(i); end; end; "
         "got = nansum(x); err = abs(got - ref);");
    auto *err = getVarPtr("err");
    EXPECT_DOUBLE_EQ(err->toScalar(), 0.0);
}

TEST_P(NanReductionTest, NanmeanLargeNDenseNaNCount)
{
    // 1024 elements, every other one NaN. The SIMD path's parallel
    // double-accumulated count must divide the SIMD-summed values
    // correctly. Reference: mean of the surviving 512 odd-indexed
    // (1-based: 1, 3, 5, ... 1023) values.
    eval("x = (1:1024)'; for i = 2:2:1024, x(i) = NaN; end; "
         "got = nanmean(x); "
         "ref_sum = 0; ref_n = 0; "
         "for i = 1:1024, "
         "  if ~isnan(x(i)), ref_sum = ref_sum + x(i); ref_n = ref_n + 1; end; "
         "end; "
         "ref = ref_sum / ref_n; err = abs(got - ref);");
    auto *err = getVarPtr("err");
    EXPECT_LT(err->toScalar(), 1e-10);
}

TEST_P(NanReductionTest, NansumMatrixDim1LargeRowsScatteredNaN)
{
    // 32-row, 3-col matrix; inject NaNs at known positions in each col,
    // verify per-column nansum matches the explicit NaN-skip sum.
    eval("R = 32; M = repmat((1:R)', 1, 3); "
         "M(5, 1) = NaN; M(15, 1) = NaN; "
         "M(8, 2) = NaN; M(20, 2) = NaN; "
         "M(1, 3) = NaN; M(31, 3) = NaN; "
         "got = nansum(M); "
         "ref = zeros(1, 3); "
         "for c = 1:3, "
         "  for r = 1:R, "
         "    if ~isnan(M(r,c)), ref(c) = ref(c) + M(r,c); end; "
         "  end; "
         "end;");
    auto *got = getVarPtr("got");
    auto *ref = getVarPtr("ref");
    EXPECT_EQ(cols(*got), 3u);
    for (size_t c = 0; c < 3; ++c)
        EXPECT_DOUBLE_EQ(got->doubleData()[c], ref->doubleData()[c])
            << "at col " << c;
}

TEST_P(NanReductionTest, NanmaxMatrixDim1LargeRowsScatteredNaN)
{
    eval("R = 32; M = repmat((1:R)', 1, 3); "
         "M(R, 1) = NaN;"  // would-be max in col 1 is now NaN; max = R-1
         "M(1:R-1, 2) = NaN; "  // col 2: only last element = R is non-NaN
         "got = nanmax(M);");
    auto *got = getVarPtr("got");
    EXPECT_DOUBLE_EQ(got->doubleData()[0], 31.0); // R - 1
    EXPECT_DOUBLE_EQ(got->doubleData()[1], 32.0); // last valid
    EXPECT_DOUBLE_EQ(got->doubleData()[2], 32.0); // no NaN, max = R
}

TEST_P(NanReductionTest, NanvarMatrixDim1LargeRowsCountNonNaN)
{
    // 32-row × 2-col; col 1 has 4 NaNs (count=28), col 2 has none.
    // Verify the SIMD per-slice two-pass variance handles a per-slice
    // count correctly.
    eval("R = 32; M = repmat((1:R)', 1, 2); "
         "M([3 10 17 24], 1) = NaN; "
         "got = nanvar(M); "
         "ref = zeros(1, 2); "
         "for c = 1:2, "
         "  s = 0; n = 0; "
         "  for r = 1:R, if ~isnan(M(r,c)), s = s + M(r,c); n = n + 1; end; end; "
         "  m = s / n; "
         "  ss = 0; "
         "  for r = 1:R, if ~isnan(M(r,c)), ss = ss + (M(r,c) - m)^2; end; end; "
         "  ref(c) = ss / (n - 1); "
         "end;");
    auto *got = getVarPtr("got");
    auto *ref = getVarPtr("ref");
    for (size_t c = 0; c < 2; ++c)
        EXPECT_NEAR(got->doubleData()[c], ref->doubleData()[c],
                    1e-10 * std::abs(ref->doubleData()[c]))
            << "at col " << c;
}

TEST_P(NanReductionTest, NansumOddSizesAroundSimdLanes)
{
    // Tail-handling under odd N: SIMD body covers floor(n/4N)*4N
    // elements, then a 1× SIMD partial, then scalar final tail.
    // x(1) and x(n) are NaN — sum(x(2:n-1)) is the reference.
    for (int n : {17, 33, 65, 127, 257}) {
        const std::string ns = std::to_string(n);
        const std::string code =
            "x = (1:" + ns + ")'; x(1) = NaN; x(" + ns + ") = NaN; "
            "got = nansum(x); "
            "ref = 0; for i = 2:" + std::to_string(n - 1) + ", ref = ref + x(i); end;";
        eval(code);
        auto *got = getVarPtr("got");
        auto *ref = getVarPtr("ref");
        EXPECT_DOUBLE_EQ(got->toScalar(), ref->toScalar()) << "at n=" << n;
    }
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

// ── Phase P1 SIMD any/all coverage ──────────────────────────
//
// These exercise the 4× unrolled SIMD scan in
// MStdLogicalReductions_simd.cpp. Cover: large-N early-exit (any), full-
// scan (all), LOGICAL-byte path (mask = x > 0 returns LOGICAL), DOUBLE
// path, and odd N around SIMD lane boundaries.

TEST_P(CumLogicalTest, AnyLargeNVectorEarlyExit)
{
    // The non-zero is buried near the end so the SIMD body has to scan
    // many full vectors before it triggers. Empirically this is the
    // worst case for early-exit; correctness check, not perf.
    eval("z = zeros(1, 1024); z(1023) = 1; r = any(z);");
    EXPECT_TRUE(evalBool("r"));
}

TEST_P(CumLogicalTest, AllLargeNFalseInTail)
{
    // Mostly-true with a single zero in the SIMD scalar tail (n = 4N+3
    // on AVX2). Catches the case where the body says "all true" but the
    // tail flips it.
    eval("z = ones(1, 35); z(34) = 0; r = all(z);");
    EXPECT_FALSE(evalBool("r"));
}

TEST_P(CumLogicalTest, AnyLogicalInputDirectByteScan)
{
    // mask is LOGICAL (output of comparison) — exercises AnyByteScan
    // (uint8 lane SIMD) instead of the DOUBLE path. The early-exit
    // must fire on the lone 1 buried near the middle.
    eval("m = (1:2048) == 1500; r = any(m);");
    EXPECT_TRUE(evalBool("r"));
    eval("r2 = all(m);");
    EXPECT_FALSE(evalBool("r2"));
}

// ── Cumulative SIMD-body coverage (Phase P6 followup) ───────
//
// Existing CumprodVector / CummaxVector / CumminVector tests use 5-8
// element vectors that don't fully cross the 4× SIMD body / scalar
// tail boundary. The cumsum/cumprod/cummax/cummin SIMD path uses a
// Hillis-Steele scan with cross-lane SlideUpLanes — got this wrong
// once with ShiftLeftLanes (AVX2 lane-crossing limitation), produced
// silently-wrong results that no test caught. These cases pin the fix
// at sizes that exercise the full body + partial-vector + scalar tail.

TEST_P(CumLogicalTest, CumsumLargeNMatchesArithmeticSequence)
{
    // x = 1..100 → cumsum is the triangular numbers k*(k+1)/2.
    eval("x = 1:100; c = cumsum(x);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(c->numel(), 100u);
    for (size_t k = 0; k < 100; ++k) {
        const double k1 = static_cast<double>(k + 1);
        const double expected = k1 * (k1 + 1.0) / 2.0;
        EXPECT_DOUBLE_EQ(c->doubleData()[k], expected) << "at k=" << k;
    }
}

TEST_P(CumLogicalTest, CumprodLargeNMatchesScalarReference)
{
    // Use small values to keep result in range. x = 1.01.^[0..63]; the
    // cumulative product builds up exponentially but stays finite.
    eval("x = 1.01 .^ (0:63); c = cumprod(x);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(c->numel(), 64u);
    double acc = 1.0;
    for (size_t k = 0; k < 64; ++k) {
        acc *= std::pow(1.01, static_cast<double>(k));
        EXPECT_NEAR(c->doubleData()[k], acc, 1e-10 * std::abs(acc))
            << "at k=" << k;
    }
}

TEST_P(CumLogicalTest, CummaxLargeNMonotonicData)
{
    // Strictly decreasing input: cummax stays at the first element.
    eval("x = (100:-1:1); c = cummax(x);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(c->numel(), 100u);
    for (size_t k = 0; k < 100; ++k)
        EXPECT_DOUBLE_EQ(c->doubleData()[k], 100.0) << "at k=" << k;
}

TEST_P(CumLogicalTest, CummaxLargeNZigzagWithKnownPeaks)
{
    // Zigzag pattern with peaks at known positions (k=0, 7, 19, 49).
    // Running max is the most-recent peak value.
    eval("x = (1:64) - 100;"            // -99..-36, all increasing
         "x([8 20 50]) = [50 75 200];"  // inject peaks (1-based)
         "c = cummax(x);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(c->numel(), 64u);
    // Before the first peak, running max follows x[0]=−99 then climbs
    // with the increasing prefix until index 7 where x = 50 takes over.
    EXPECT_DOUBLE_EQ(c->doubleData()[0], -99.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[6], -93.0); // max(x[0..6]) climb
    EXPECT_DOUBLE_EQ(c->doubleData()[7], 50.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[18], 50.0); // until next peak
    EXPECT_DOUBLE_EQ(c->doubleData()[19], 75.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[49], 200.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[63], 200.0); // stays at 200
}

TEST_P(CumLogicalTest, CumminLargeNZigzag)
{
    // Mirror of cummax with min: troughs at the same indices.
    eval("x = -((1:64) - 100);"          // 99..36, all decreasing
         "x([8 20 50]) = [-50 -75 -200];"
         "c = cummin(x);");
    auto *c = getVarPtr("c");
    EXPECT_DOUBLE_EQ(c->doubleData()[0], 99.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[7], -50.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[19], -75.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[49], -200.0);
}

TEST_P(CumLogicalTest, CumOdSizesAroundSimdLanes)
{
    // Sizes just past 1×, 2×, 4× the SIMD lane count to exercise the
    // body + partial-SIMD + scalar-tail combinations.
    for (int n : {17, 33, 65, 127}) {
        const std::string ns = std::to_string(n);
        // Reference cumsum of 1..n: triangular numbers.
        eval("x = 1:" + ns + "; c = cumsum(x);");
        auto *c = getVarPtr("c");
        ASSERT_EQ(c->numel(), static_cast<size_t>(n));
        for (size_t k = 0; k < static_cast<size_t>(n); ++k) {
            const double k1 = static_cast<double>(k + 1);
            const double expected = k1 * (k1 + 1.0) / 2.0;
            EXPECT_DOUBLE_EQ(c->doubleData()[k], expected)
                << "cumsum at n=" << n << " k=" << k;
        }
    }
}

TEST_P(CumLogicalTest, CumsumMatrixDim1LargeRowsPerColumn)
{
    // 64-row × 3-col matrix. cumsum(M, 1) scans down each column via
    // the SIMD prefix-sum kernel — slice = 64 elements > AVX2 lane (4),
    // so the body fires per column.
    eval("R = 64; M = repmat((1:R)', 1, 3) + 100 * repmat(0:2, R, 1); "
         "C = cumsum(M, 1);");
    auto *C = getVarPtr("C");
    EXPECT_EQ(rows(*C), 64u);
    EXPECT_EQ(cols(*C), 3u);
    // For column k: M(:,k) = (1..64) + 100*k; cumsum at row r is
    //   sum_{i=1..r+1} (i + 100*k) = (r+1)*(r+2)/2 + 100*k*(r+1).
    for (size_t r = 0; r < 64; ++r)
        for (size_t c = 0; c < 3; ++c) {
            const double r1 = static_cast<double>(r + 1);
            const double expected = r1 * (r1 + 1.0) / 2.0
                                  + 100.0 * static_cast<double>(c) * r1;
            EXPECT_DOUBLE_EQ((*C)(r, c), expected) << "at r=" << r << " c=" << c;
        }
}

TEST_P(CumLogicalTest, CummaxMatrixDim1LargeRowsPerColumn)
{
    // Increasing-then-flat per-column: column k = (1..64). cummax = column itself.
    eval("R = 64; M = repmat((1:R)', 1, 3); C = cummax(M, 1);");
    auto *C = getVarPtr("C");
    for (size_t r = 0; r < 64; ++r)
        for (size_t c = 0; c < 3; ++c)
            EXPECT_DOUBLE_EQ((*C)(r, c), static_cast<double>(r + 1))
                << "at r=" << r << " c=" << c;
}

TEST_P(CumLogicalTest, Cumprod3DDim1LargePerPagePerColumn)
{
    // 16-row × 2-col × 2-page array; cumprod along dim=1 routes every
    // column on every page through the SIMD prefix-prod kernel.
    eval("R = 16; "
         "M = ones(R, 2, 2); "
         "for p = 1:2, M(:,:,p) = 1.05; end; "  // all 1.05
         "C = cumprod(M, 1);");
    auto *C = getVarPtr("C");
    EXPECT_EQ(rows(*C), 16u);
    EXPECT_EQ(C->dims().pages(), 2u);
    // cumprod of constant 1.05 is 1.05^(r+1).
    for (size_t p = 0; p < 2; ++p)
        for (size_t c = 0; c < 2; ++c)
            for (size_t r = 0; r < 16; ++r) {
                const double expected = std::pow(1.05, static_cast<double>(r + 1));
                EXPECT_NEAR((*C)(r, c, p), expected,
                            1e-12 * std::abs(expected))
                    << "at r=" << r << " c=" << c << " p=" << p;
            }
}

TEST_P(CumLogicalTest, AnyAllOddSizesAroundSimdLanes)
{
    // SIMD body covers floor(n/4N)*4N elements, then a 1× SIMD partial,
    // then scalar tail. Verify any/all decisions across that boundary.
    for (int n : {17, 33, 65, 127, 257}) {
        const std::string code =
            "x = zeros(1, " + std::to_string(n) + "); "
            "y = any(x); z = all(ones(1, " + std::to_string(n) + "));";
        eval(code);
        EXPECT_FALSE(evalBool("y")) << "any(zeros) at n=" << n;
        EXPECT_TRUE (evalBool("z")) << "all(ones) at n=" << n;
    }
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
