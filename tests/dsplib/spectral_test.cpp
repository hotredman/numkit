// tests/spectral_test.cpp

#include "MEngine.hpp"
#include "MStdLibrary.hpp"
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace mlab;

class SpectralTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { StdLibrary::install(engine); }
    MValue eval(const std::string &code) { return engine.eval(code); }
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
