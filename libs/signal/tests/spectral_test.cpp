// tests/spectral_test.cpp

#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class SpectralTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ============================================================
// periodogram
// ============================================================

TEST_F(SpectralTest, PeriodogramOutputLengths)
{
    eval("[Pxx, F] = periodogram(randn(1, 64));");
    // nfft = nextpow2(64) = 64, nOut = 64/2+1 = 33
    EXPECT_EQ(eval("Pxx").numel(), 33u);
    EXPECT_EQ(eval("F").numel(), 33u);
}

TEST_F(SpectralTest, PeriodogramNonnegative)
{
    eval("[Pxx, F] = periodogram(randn(1, 128));");
    double minVal = evalScalar("min(Pxx)");
    EXPECT_GE(minVal, 0.0);
}

TEST_F(SpectralTest, PeriodogramFrequencyRange)
{
    eval("[Pxx, F] = periodogram(randn(1, 64));");
    EXPECT_NEAR(evalScalar("F(1)"), 0.0, 1e-10);
    EXPECT_NEAR(evalScalar("F(33)"), M_PI, 0.1); // approximately pi
}

TEST_F(SpectralTest, PeriodogramPeakAtDc)
{
    // Constant signal → all energy at DC
    eval("[Pxx, F] = periodogram(5 * ones(1, 64));");
    eval("[mx, idx] = max(Pxx);");
    EXPECT_DOUBLE_EQ(evalScalar("idx"), 1.0); // DC bin
}

TEST_F(SpectralTest, PeriodogramWithWindow)
{
    eval("w = hamming(64);");
    eval("[Pxx, F] = periodogram(randn(1, 64), w);");
    EXPECT_EQ(eval("Pxx").numel(), 33u);
}

// ============================================================
// pwelch
// ============================================================

TEST_F(SpectralTest, PwelchOutputLengths)
{
    eval("[Pxx, F] = pwelch(randn(1, 512));");
    // Default: winLen=256, nfft=256, nOut=129
    EXPECT_EQ(eval("Pxx").numel(), 129u);
    EXPECT_EQ(eval("F").numel(), 129u);
}

TEST_F(SpectralTest, PwelchNonnegative)
{
    eval("[Pxx, F] = pwelch(randn(1, 1024));");
    EXPECT_GE(evalScalar("min(Pxx)"), 0.0);
}

TEST_F(SpectralTest, PwelchSmootherThanPeriodogram)
{
    // Welch should give smoother estimate → lower coefficient of variation
    // Use same nfft for both so output lengths are comparable
    eval("x = randn(1, 1024);");
    eval("w = hamming(256);");
    eval("[P1, ~] = periodogram(x, rectwin(1024), 1024);");
    eval("[P2, ~] = pwelch(x, w, 128, 256);");
    // Compare coefficient of variation (std/mean) — scale-independent
    double cvPeriod = evalScalar("sqrt(sum((P1 - mean(P1)).^2) / length(P1)) / mean(P1)");
    double cvWelch = evalScalar("sqrt(sum((P2 - mean(P2)).^2) / length(P2)) / mean(P2)");
    EXPECT_LT(cvWelch, cvPeriod);
}

// ============================================================
// spectrogram
// ============================================================

TEST_F(SpectralTest, SpectrogramOutputDimensions)
{
    eval("[S, F, T] = spectrogram(randn(1, 1024), 256, 128, 256);");
    // nFreqs = 256/2+1 = 129
    auto S = eval("S");
    EXPECT_EQ(S.dims().rows(), 129u);
    // nSegments = floor((1024-256)/(256-128)) + 1 = 7
    EXPECT_EQ(S.dims().cols(), 7u);
}

TEST_F(SpectralTest, SpectrogramIsComplex)
{
    eval("[S, F, T] = spectrogram(randn(1, 512), 128, 64, 128);");
    EXPECT_TRUE(eval("S").isComplex());
}

TEST_F(SpectralTest, SpectrogramFrequencyVector)
{
    eval("[S, F, T] = spectrogram(randn(1, 512), 128, 64, 128);");
    EXPECT_NEAR(evalScalar("F(1)"), 0.0, 1e-10);
    size_t nFreqs = eval("F").numel();
    EXPECT_EQ(nFreqs, 65u); // 128/2+1
}

TEST_F(SpectralTest, SpectrogramTimeVector)
{
    eval("[S, F, T] = spectrogram(randn(1, 512), 128, 64, 128);");
    // Each time point is center of segment
    EXPECT_NEAR(evalScalar("T(1)"), 64.0, 1.0); // winLen/2
}

// ── Levinson-Durbin: valid + non-PSD recursion ───────────────────────
// Valid PSD autocorrelation: standard AR fit (vs MATLAB R2025b).
TEST_F(SpectralTest, LevinsonValidPsd)
{
    eval("[a, e, k] = levinson([1 0.6 0.3 0.1], 3);");
    EXPECT_NEAR(evalScalar("a(2)"), -0.650246305419, 1e-12);
    EXPECT_NEAR(evalScalar("a(4)"),  0.064039408867, 1e-12);
    EXPECT_NEAR(evalScalar("e"),     0.631773399015, 1e-12);
    EXPECT_NEAR(evalScalar("k(1)"), -0.6,            1e-12);
}

// Non-PSD autocorrelation (|k(2)|>1): MATLAB runs the full recursion
// through negative residual energy; numkit used to early-exit at e<=0,
// zeroing the tail of a/k and returning e=0. Regression guard.
TEST_F(SpectralTest, LevinsonNonPsdRunsFullRecursion)
{
    eval("[a, e, k] = levinson([4 -2 -3 1 1.5], 3);");
    EXPECT_NEAR(evalScalar("a(2)"), -1.785714285714, 1e-10);
    EXPECT_NEAR(evalScalar("a(3)"), -1.25,           1e-10);
    EXPECT_NEAR(evalScalar("a(4)"), -2.214285714286, 1e-10);
    EXPECT_NEAR(evalScalar("e"),     9.107142857143, 1e-10);
    EXPECT_NEAR(evalScalar("k(3)"), -2.214285714286, 1e-10);
}

// poly2rc second output R0 = efinal / prod(1 - k.^2). numkit used to
// return only k (no R0). vs MATLAB R2025b.
TEST_F(SpectralTest, Poly2rcReturnsR0)
{
    eval("[k, r0] = poly2rc([1 0.6 0.2 -0.1], 4);");
    EXPECT_NEAR(evalScalar("k(1)"),  0.496,          1e-12);
    EXPECT_NEAR(evalScalar("k(2)"),  0.262626262626, 1e-12);
    EXPECT_NEAR(evalScalar("k(3)"), -0.1,            1e-12);
    EXPECT_NEAR(evalScalar("r0"),    5.755726948310, 1e-9);
}

// rc2poly second output efinal = r0 * prod(1 - k.^2) (inverse of poly2rc's
// R0). numkit used to return only a (no efinal). vs MATLAB R2025b.
TEST_F(SpectralTest, Rc2polyReturnsEfinal)
{
    eval("a = rc2poly([-0.5 0.4 0.2]);");
    EXPECT_NEAR(evalScalar("a(2)"), -0.62, 1e-12);
    EXPECT_NEAR(evalScalar("a(3)"),  0.26, 1e-12);
    EXPECT_NEAR(evalScalar("a(4)"),  0.20, 1e-12);
    eval("[a2, e2] = rc2poly([0.5 0.3], 4);");
    EXPECT_NEAR(evalScalar("a2(2)"), 0.65, 1e-12);
    EXPECT_NEAR(evalScalar("a2(3)"), 0.30, 1e-12);
    EXPECT_NEAR(evalScalar("e2"),    2.73, 1e-12);   // 4*(1-0.25)*(1-0.09)
}

// ac2rc on a non-trivial autocorrelation: reflection coeffs match MATLAB
// (an earlier spec claimed a KNOWN GAP on a degenerate input — none here).
TEST_F(SpectralTest, Ac2rcReflectionCoeffs)
{
    eval("k = ac2rc([4 1 -0.5 0.3]);");
    EXPECT_NEAR(evalScalar("k(1)"), -0.25,           1e-12);
    EXPECT_NEAR(evalScalar("k(2)"),  0.2,            1e-12);
    EXPECT_NEAR(evalScalar("k(3)"), -0.180555555556, 1e-10);
}
