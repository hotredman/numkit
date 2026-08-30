// tests/spectral_test.cpp

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class SpectralTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ============================================================
// periodogram
// ============================================================

TEST_F(SpectralTest, PeriodogramOutputLengths)
{
    eval("[Pxx, F] = periodogram(randn(1, 64));");
    // MATLAB default nfft = max(256, 2^nextpow2(64)) = 256, nOut = 256/2+1 = 129
    EXPECT_EQ(eval("Pxx").numel(), 129u);
    EXPECT_EQ(eval("F").numel(), 129u);
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
    EXPECT_NEAR(evalScalar("F(129)"), M_PI, 1e-10); // Nyquist (nfft 256 -> 129 bins)
}

TEST_F(SpectralTest, PeriodogramPeakAtDc)
{
    // Constant signal → energy concentrated at the lowest frequencies. With
    // the MATLAB default nfft=256 the 64-sample signal is zero-padded, so the
    // Dirichlet main-lobe peak lands at bin 2 (matches MATLAB R2025b exactly).
    eval("[Pxx, F] = periodogram(5 * ones(1, 64));");
    eval("[mx, idx] = max(Pxx);");
    EXPECT_DOUBLE_EQ(evalScalar("idx"), 2.0);
}

TEST_F(SpectralTest, PeriodogramWithWindow)
{
    eval("w = hamming(64);");
    eval("[Pxx, F] = periodogram(randn(1, 64), w);");
    EXPECT_EQ(eval("Pxx").numel(), 129u);  // nfft default 256 -> 129 bins
}

// bugs/signal/periodogram-pxxc.md — chi-square confidence interval (3rd output).
// Reference values from MATLAB R2025b:
//   [pxx,f,pxxc] = periodogram((1:8)', rectwin(8), 8, 1, 'ConfidenceLevel', 0.95)
// Each PSD bin ~ chi-square with v DOF: interior bins v=2, the real DC and
// (even nfft) Nyquist bins v=1. pxxc = pxx .* v ./ chi2inv([1-a/2, a/2], v).
TEST_F(SpectralTest, PeriodogramConfidenceIntervalValues)
{
    eval("[pxx, f, pxxc] = periodogram((1:8)', rectwin(8), 8, 1, 'ConfidenceLevel', 0.95);");
    EXPECT_EQ(eval("pxx").numel(), 5u);
    ASSERT_EQ(static_cast<int>(evalScalar("size(pxxc,1)")), 5);
    ASSERT_EQ(static_cast<int>(evalScalar("size(pxxc,2)")), 2);

    // Base PSD (confirms the reference setup matches MATLAB).
    EXPECT_NEAR(evalScalar("pxx(1)"), 162.0,         1e-9);
    EXPECT_NEAR(evalScalar("pxx(2)"),  27.313708499, 1e-7);

    // Lower bounds — bins 1,5 (DC, Nyquist) 1 DOF; bins 2,3,4 interior 2 DOF.
    EXPECT_NEAR(evalScalar("pxxc(1,1)"), 32.2459534233,  1e-7);
    EXPECT_NEAR(evalScalar("pxxc(2,1)"),  7.40433750648, 1e-8);
    EXPECT_NEAR(evalScalar("pxxc(3,1)"),  2.16868024545, 1e-8);
    EXPECT_NEAR(evalScalar("pxxc(5,1)"),  0.398098190411, 1e-9);
    // Upper bounds.
    EXPECT_NEAR(evalScalar("pxxc(1,2)"), 164957.839695,  1e-3);
    EXPECT_NEAR(evalScalar("pxxc(2,2)"),   1078.83385939, 1e-5);
    EXPECT_NEAR(evalScalar("pxxc(5,2)"),   2036.51653944, 1e-5);
}

TEST_F(SpectralTest, PeriodogramConfidenceDefaultLevel)
{
    // No 'ConfidenceLevel' name-value → MATLAB default 0.95; pxxc still emitted
    // once a 3rd output is requested.
    eval("[pxx, f, pxxc] = periodogram((1:8)', rectwin(8), 8, 1);");
    ASSERT_EQ(static_cast<int>(evalScalar("size(pxxc,2)")), 2);
    EXPECT_NEAR(evalScalar("pxxc(2,1)"), 7.40433750648, 1e-8);
    // Interior lower-bound ratio == 2/chi2inv(0.975,2).
    EXPECT_NEAR(evalScalar("pxxc(3,1)/pxx(3)"), 0.2710850307, 1e-9);
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

// pwelch(x,[],[],nfft): empty placeholders must select defaults, not error
// ("Cannot convert double to scalar"). Output must equal the nfft path. vs MATLAB.
TEST_F(SpectralTest, PwelchEmptyPlaceholders)
{
    eval("x = cos(2*pi*0.1*(0:127));");
    eval("[p, f] = pwelch(x, [], [], 128);");
    EXPECT_EQ(eval("p").numel(), 65u);            // nfft/2 + 1
    // dominant bin (14) holds the peak; value matches MATLAB R2025b.
    EXPECT_NEAR(evalScalar("p(14)"), 1.59267, 1e-4);
    EXPECT_NEAR(evalScalar("max(p)"), 1.59267, 1e-4);
    // cpsd / mscohere / tfestimate accept the same empty placeholders.
    EXPECT_NO_THROW(eval("cpsd(x, x, [], [], 128);"));
    EXPECT_NO_THROW(eval("mscohere(x, x, [], [], 128);"));
    EXPECT_NO_THROW(eval("tfestimate(x, x, [], [], 128);"));
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
    // With no fs, MATLAB uses the normalized convention fs = 2*pi, so the
    // segment-centre time is centre/(2*pi). First centre = winLen/2 = 64.
    eval("[S, F, T] = spectrogram(randn(1, 512), 128, 64, 128);");
    EXPECT_NEAR(evalScalar("T(1)"), 64.0 / (2.0 * 3.14159265358979323846), 1e-9);
    // With an explicit fs the time axis is in seconds: centre/fs.
    eval("[S2, F2, T2] = spectrogram(randn(1, 512), 128, 64, 128, 100);");
    EXPECT_NEAR(evalScalar("T2(1)"), 0.64, 1e-12);    // 64/100
    EXPECT_NEAR(evalScalar("F2(end)"), 50.0, 1e-12);  // fs/2 = Nyquist
    EXPECT_NEAR(evalScalar("F2(2)"), 100.0 / 128.0, 1e-12);
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

// ============================================================
// spectrogram 4th output (ps = power spectral density) — DEEP-PROBE 2026-06
// (was "Too many output arguments"). vs MATLAB R2025b.
// ============================================================

TEST_F(SpectralTest, SpectrogramPsdFourthOutput)
{
    // Explicit 8-pt Hamming window, 4 overlap, nfft 16, fs 100.
    eval("[s, f, t, ps] = spectrogram((1:64)', 8, 4, 16, 100);");
    EXPECT_EQ(eval("ps").dims().rows(), 9u);   // one-sided: nfft/2+1
    EXPECT_NEAR(evalScalar("ps(1,1)"), 1.08212, 1e-4);   // DC bin (c=1)
    EXPECT_NEAR(evalScalar("ps(3,2)"), 1.98718, 1e-4);   // interior (c=2)
    EXPECT_NEAR(evalScalar("sum(ps(:))"), 3254.62, 1e-1);
    // PSD is real and nonnegative.
    EXPECT_GE(evalScalar("min(ps(:))"), 0.0);
}

TEST_F(SpectralTest, SpectrogramPsdDefaultWindow)
{
    // No fs / window: default hamming(floor(nx/4.5)), fs=2*pi, nfft=128.
    eval("[s2, f2, t2, ps2] = spectrogram((1:50)');");
    EXPECT_EQ(eval("ps2").dims().rows(), 129u);
    EXPECT_NEAR(evalScalar("ps2(2,2)"), 344.941, 1e-2);
}
