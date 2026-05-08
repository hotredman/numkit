// libs/signal/tests/dspgaps_test.cpp
// Phase 9: medfilt1 / findpeaks / goertzel / dct / idct

#include "dual_engine_fixture.hpp"
#include <cmath>

using namespace m_test;

class DspGapsTest : public DualEngineTest
{};

// ── medfilt1 ────────────────────────────────────────────────

TEST_P(DspGapsTest, Medfilt1RemovesSingleSpike)
{
    // [1 1 100 1 1] with k=3 → spike removed at center
    eval("y = medfilt1([1 1 100 1 1], 3);");
    auto *y = getVarPtr("y");
    EXPECT_EQ(y->numel(), 5u);
    EXPECT_DOUBLE_EQ(y->doubleData()[2], 1.0);  // spike replaced by median
}

TEST_P(DspGapsTest, Medfilt1DefaultK3)
{
    // No second arg → k=3
    eval("y = medfilt1([1 1 100 1 1]);");
    auto *y = getVarPtr("y");
    EXPECT_DOUBLE_EQ(y->doubleData()[2], 1.0);
}

TEST_P(DspGapsTest, Medfilt1OddWindow)
{
    // window=5 over [3 1 4 1 5 9 2 6]
    eval("y = medfilt1([3 1 4 1 5 9 2 6], 5);");
    auto *y = getVarPtr("y");
    EXPECT_EQ(y->numel(), 8u);
    // For y[3] (i=3) window covers indices 1..5 -> [1,4,1,5,9] sorted [1,1,4,5,9] median=4
    EXPECT_DOUBLE_EQ(y->doubleData()[3], 4.0);
    // y[4] (i=4) window covers 2..6 -> [4,1,5,9,2] sorted [1,2,4,5,9] median=4
    EXPECT_DOUBLE_EQ(y->doubleData()[4], 4.0);
}

TEST_P(DspGapsTest, Medfilt1BoundaryTruncation)
{
    // Boundary: window truncated, output still valid
    eval("y = medfilt1([10 1 1 1 1], 3);");
    auto *y = getVarPtr("y");
    // y[0] window=[10,1] median=5.5? Or y[0] window=[10,1] (truncated) — even-length avg.
    // With k=3, leftHalf=1, rightHalf=1. At i=0: window [src[0..1]] = [10, 1] (size 2).
    // Even median = 0.5*(min+max) = 5.5.
    EXPECT_DOUBLE_EQ(y->doubleData()[0], 5.5);
}

TEST_P(DspGapsTest, Medfilt1PreservesShape)
{
    // Empty
    eval("y = medfilt1([]);");
    auto *y = getVarPtr("y");
    EXPECT_EQ(y->numel(), 0u);
}

// ── findpeaks ───────────────────────────────────────────────

TEST_P(DspGapsTest, FindpeaksBasic)
{
    // [1 3 2 5 4 1] — peaks at i=2 (value 3) and i=4 (value 5)
    eval("function [a, b] = wrap(x)\n"
         "  [a, b] = findpeaks(x);\n"
         "end");
    eval("[v, idx] = wrap([1 3 2 5 4 1]);");
    auto *v   = getVarPtr("v");
    auto *idx = getVarPtr("idx");
    EXPECT_EQ(v->numel(), 2u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(idx->doubleData()[0], 2.0);  // 1-based
    EXPECT_DOUBLE_EQ(idx->doubleData()[1], 4.0);
}

TEST_P(DspGapsTest, FindpeaksNoPeaks)
{
    // Monotonic ramp — no peaks
    eval("v = findpeaks([1 2 3 4 5]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 0u);
}

TEST_P(DspGapsTest, FindpeaksEdgeNotAPeak)
{
    // First and last positions never count, even if they look like peaks
    eval("v = findpeaks([10 5 2 5 10]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 0u);
}

TEST_P(DspGapsTest, FindpeaksFlatTopNotPeak)
{
    // [1 3 3 3 1] — strict-greater requirement → no peak
    eval("v = findpeaks([1 3 3 3 1]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 0u);
}

// ── goertzel ────────────────────────────────────────────────

TEST_P(DspGapsTest, GoertzelMatchesDFTBin)
{
    // Single sinusoid at bin k=2 (out of N=8): cos(2π*1*n/8)
    // → DFT[1] = N/2 = 4 (real, non-DC bin)
    eval("N = 8; n = 0:N-1; x = cos(2*pi*1*n/N); g = goertzel(x, 2);");
    auto *g = getVarPtr("g");
    // g should be complex; real part ≈ N/2, imag part ≈ 0
    EXPECT_TRUE(g->isComplex());
    const double re = g->complexData()[0].real();
    const double im = g->complexData()[0].imag();
    EXPECT_NEAR(re, 4.0, 1e-9);
    EXPECT_NEAR(im, 0.0, 1e-9);
}

TEST_P(DspGapsTest, GoertzelDC)
{
    // DC bin: sum of inputs
    eval("g = goertzel([1 2 3 4], 1);");
    auto *g = getVarPtr("g");
    EXPECT_TRUE(g->isComplex());
    EXPECT_NEAR(g->complexData()[0].real(), 10.0, 1e-12);
    EXPECT_NEAR(g->complexData()[0].imag(), 0.0, 1e-12);
}

TEST_P(DspGapsTest, GoertzelMultipleBins)
{
    eval("g = goertzel([1 2 3 4], [1 2]);");
    auto *g = getVarPtr("g");
    EXPECT_EQ(g->numel(), 2u);
    EXPECT_NEAR(g->complexData()[0].real(), 10.0, 1e-12);
}

// ── dct / idct ──────────────────────────────────────────────

TEST_P(DspGapsTest, DctConstantSignal)
{
    // dct of [1 1 1 1] (length 4): only X[0] non-zero = sqrt(1/4)*4 = 2
    eval("X = dct([1 1 1 1]);");
    auto *X = getVarPtr("X");
    EXPECT_EQ(X->numel(), 4u);
    EXPECT_NEAR(X->doubleData()[0], 2.0, 1e-12);
    EXPECT_NEAR(X->doubleData()[1], 0.0, 1e-12);
    EXPECT_NEAR(X->doubleData()[2], 0.0, 1e-12);
    EXPECT_NEAR(X->doubleData()[3], 0.0, 1e-12);
}

TEST_P(DspGapsTest, DctIdctRoundTrip)
{
    // idct(dct(x)) ≈ x
    eval("x = [3 1 4 1 5 9 2 6]; y = idct(dct(x));");
    auto *y = getVarPtr("y");
    auto *x = getVarPtr("x");
    EXPECT_EQ(y->numel(), x->numel());
    for (size_t i = 0; i < x->numel(); ++i)
        EXPECT_NEAR(y->doubleData()[i], x->doubleData()[i], 1e-10);
}

TEST_P(DspGapsTest, DctParseval)
{
    // Energy preserved: sum(x^2) ≈ sum(X^2)
    eval("x = [1 2 3 4 5]; X = dct(x); ex = sum(x.^2); eX = sum(X.^2);");
    EXPECT_NEAR(getVar("ex"), getVar("eX"), 1e-10);
}

TEST_P(DspGapsTest, DctSinglePoint)
{
    // Length-1 dct of [5] = [5]
    eval("X = dct([5]);");
    auto *X = getVarPtr("X");
    EXPECT_EQ(X->numel(), 1u);
    EXPECT_NEAR(X->doubleData()[0], 5.0, 1e-12);
}

// Bug fix 2026-05-08 — dct/idct were not handling matrix input column-wise,
// length-override, or dim arg. Coverage tests for the fixes:

TEST_P(DspGapsTest, DctMatrixColumnWise)
{
    // dct(M) on a matrix transforms each column independently.
    // M = [1 5; 2 6; 3 7; 4 8]:  col1=[1 2 3 4], col2=[5 6 7 8].
    eval("M = [1 5; 2 6; 3 7; 4 8]; Y = dct(M);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_EQ(Y->dims().rows(), 4u);
    EXPECT_EQ(Y->dims().cols(), 2u);
    // Column 1: dct([1 2 3 4]')
    EXPECT_NEAR(evalScalar("Y(1,1)"),  5.0,                 1e-12);
    EXPECT_NEAR(evalScalar("Y(2,1)"), -2.2304424973876635,   1e-12);
    EXPECT_NEAR(evalScalar("Y(4,1)"), -0.1585126677811071,   1e-12);
    // Column 2: dct([5 6 7 8]')
    EXPECT_NEAR(evalScalar("Y(1,2)"), 13.0,                  1e-12);
    EXPECT_NEAR(evalScalar("Y(2,2)"), -2.2304424973876635,   1e-12);
}

TEST_P(DspGapsTest, DctLengthOverrideTruncate)
{
    // dct(x, 6) on length-8 input truncates to 6 before transform.
    eval("y = dct((1:8)', 6);");
    auto *y = getVarPtr("y");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(y->numel(), 6u);
    EXPECT_NEAR(evalScalar("y(1)"), 8.5732141, 1e-6);
    EXPECT_NEAR(evalScalar("y(2)"), -4.1625611, 1e-6);
}

TEST_P(DspGapsTest, DctLengthOverridePad)
{
    // dct(x, 10) zero-pads from length-8 to 10.
    eval("y = dct((1:8)', 10);");
    auto *y = getVarPtr("y");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(y->numel(), 10u);
    EXPECT_NEAR(evalScalar("y(1)"),  11.3842, 1e-3);
    EXPECT_NEAR(evalScalar("y(10)"), -1.1635, 1e-3);
}

TEST_P(DspGapsTest, DctRowWiseDim2)
{
    // dct(M, 4, 2) transforms each row.
    eval("M = [1 5; 2 6; 3 7; 4 8]; Y = dct(M, 4, 2);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_EQ(Y->dims().rows(), 4u);
    EXPECT_EQ(Y->dims().cols(), 4u);
    // Row 1: dct([1 5 0 0], 4) — pad to 4.
    EXPECT_NEAR(evalScalar("Y(1,1)"),  3.0,    1e-9);
    EXPECT_NEAR(evalScalar("Y(1,4)"), -2.9958, 1e-3);
}

TEST_P(DspGapsTest, IdctMatrixRoundTrip)
{
    eval("M = [1 5; 2 6; 3 7; 4 8]; R = idct(dct(M));");
    auto *R = getVarPtr("R");
    EXPECT_NEAR(evalScalar("R(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("R(2,2)"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("R(4,1)"), 4.0, 1e-12);
    EXPECT_NEAR(evalScalar("R(4,2)"), 8.0, 1e-12);
}

TEST_P(DspGapsTest, DctTypeOtherThan2Errors)
{
    // 'Type' values other than 2 are not yet implemented; explicit
    // error instead of silently doing Type-II.
    bool threw = false;
    try { eval("dct((1:8)', 'Type', 1);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

INSTANTIATE_DUAL(DspGapsTest);
