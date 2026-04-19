// tests/filter_design_test.cpp

#include "MEngine.hpp"
#include "MStdLibrary.hpp"
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class FilterDesignTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { StdLibrary::install(engine); }
    MValue eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ============================================================
// butter
// ============================================================

TEST_F(FilterDesignTest, ButterReturnsCorrectLength)
{
    // Nth order → N+1 coefficients
    eval("[b, a] = butter(4, 0.5);");
    EXPECT_EQ(eval("b").numel(), 5u);
    EXPECT_EQ(eval("a").numel(), 5u);
}

TEST_F(FilterDesignTest, ButterA0IsOne)
{
    eval("[b, a] = butter(3, 0.3);");
    EXPECT_NEAR(evalScalar("a(1)"), 1.0, 1e-10);
}

TEST_F(FilterDesignTest, ButterLowpassUnityDcGain)
{
    // Lowpass: sum(b)/sum(a) should be ~1 at DC
    eval("[b, a] = butter(4, 0.5);");
    double dcGain = evalScalar("abs(sum(b) / sum(a))");
    EXPECT_NEAR(dcGain, 1.0, 1e-6);
}

TEST_F(FilterDesignTest, ButterHighpass)
{
    eval("[b, a] = butter(4, 0.5, 'high');");
    EXPECT_EQ(eval("b").numel(), 5u);
    // DC gain should be ~0 for highpass
    double dcGain = evalScalar("abs(sum(b) / sum(a))");
    EXPECT_NEAR(dcGain, 0.0, 0.1);
}

TEST_F(FilterDesignTest, ButterHighpassNyquistGain)
{
    // At Nyquist, highpass gain ≈ 1
    eval("[b, a] = butter(4, 0.5, 'high');");
    // Evaluate at z = -1: alternate signs
    eval("bg = 0; ag = 0;");
    eval("for k = 1:length(b); bg = bg + b(k) * (-1)^(k-1); end;");
    eval("for k = 1:length(a); ag = ag + a(k) * (-1)^(k-1); end;");
    double nyqGain = evalScalar("abs(bg / ag)");
    EXPECT_NEAR(nyqGain, 1.0, 0.1);
}

TEST_F(FilterDesignTest, ButterCombinedWithFilter)
{
    // Filter a DC signal with lowpass → should pass through
    eval("[b, a] = butter(2, 0.5);");
    eval("x = ones(1, 100);");
    eval("y = filter(b, a, x);");
    // Steady-state output should approach 1.0
    EXPECT_NEAR(evalScalar("y(100)"), 1.0, 0.01);
}

// ============================================================
// fir1
// ============================================================

TEST_F(FilterDesignTest, Fir1Length)
{
    // fir1(N, Wn) returns N+1 coefficients
    auto r = eval("fir1(20, 0.5)");
    EXPECT_EQ(r.numel(), 21u);
}

TEST_F(FilterDesignTest, Fir1LowpassDcGain)
{
    eval("b = fir1(30, 0.5);");
    double dcGain = evalScalar("sum(b)");
    EXPECT_NEAR(dcGain, 1.0, 0.01);
}

TEST_F(FilterDesignTest, Fir1Symmetric)
{
    // Linear-phase FIR: symmetric coefficients
    eval("b = fir1(20, 0.3);");
    for (int i = 1; i <= 10; ++i) {
        std::string l = "b(" + std::to_string(i) + ")";
        std::string r = "b(" + std::to_string(22 - i) + ")";
        EXPECT_NEAR(evalScalar(l), evalScalar(r), 1e-10);
    }
}

TEST_F(FilterDesignTest, Fir1Highpass)
{
    eval("b = fir1(30, 0.5, 'high');");
    // DC gain should be ~0
    double dcGain = evalScalar("abs(sum(b))");
    EXPECT_NEAR(dcGain, 0.0, 0.01);
}

// ============================================================
// freqz
// ============================================================

TEST_F(FilterDesignTest, FreqzOutputLengths)
{
    eval("[H, W] = freqz([1 1], [1], 128);");
    EXPECT_EQ(eval("H").numel(), 128u);
    EXPECT_EQ(eval("W").numel(), 128u);
}

TEST_F(FilterDesignTest, FreqzFrequencyRange)
{
    eval("[H, W] = freqz([1], [1], 256);");
    EXPECT_NEAR(evalScalar("W(1)"), 0.0, 1e-10);
    EXPECT_NEAR(evalScalar("W(256)"), M_PI, 1e-10);
}

TEST_F(FilterDesignTest, FreqzUnitGainForPassthrough)
{
    // b=[1], a=[1] → H(w) = 1 for all w
    eval("[H, W] = freqz([1], [1], 64);");
    eval("Hmag = abs(H);");
    for (int i = 1; i <= 64; ++i) {
        std::string hi = "Hmag(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(hi), 1.0, 1e-10);
    }
}

TEST_F(FilterDesignTest, FreqzButterAtCutoff)
{
    // At cutoff, Butterworth gain ≈ -3dB ≈ 0.707
    eval("[b, a] = butter(4, 0.5);");
    eval("[H, W] = freqz(b, a, 512);");
    eval("Hmag = abs(H);");
    // Find index closest to Wn*pi = 0.5*pi
    eval("target = 0.5 * pi;");
    eval("[~, idx] = min(abs(W - target));");
    double gainAtCutoff = evalScalar("Hmag(idx)");
    EXPECT_NEAR(gainAtCutoff, 1.0 / std::sqrt(2.0), 0.05);
}
