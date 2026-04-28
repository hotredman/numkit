// tests/filter_design_test.cpp

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit::m;

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

// ============================================================
// phasez
// ============================================================

TEST_F(FilterDesignTest, PhasezPassthroughIsZero)
{
    // b=[1], a=[1] → H(w) = 1 for all w → phase = 0.
    eval("[phi, W] = phasez([1], [1], 64);");
    for (int i = 1; i <= 64; ++i) {
        std::string p = "phi(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(p), 0.0, 1e-12);
    }
}

TEST_F(FilterDesignTest, PhasezReturnsCorrectShape)
{
    eval("[phi, W] = phasez([1 -0.5], [1], 128);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(phi)"), 128.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(W)"), 128.0);
    EXPECT_NEAR(evalScalar("W(1)"), 0.0, 1e-10);
    EXPECT_NEAR(evalScalar("W(128)"), M_PI, 1e-10);
}

TEST_F(FilterDesignTest, PhasezPureDelayFilterIsLinear)
{
    // b = [0 0 0 1] (3-sample delay), a = [1] → phase = -3·w (linear, slope = -3).
    eval("[phi, W] = phasez([0 0 0 1], [1], 256);");
    double slope = evalScalar("(phi(200) - phi(50)) / (W(200) - W(50));");
    EXPECT_NEAR(slope, -3.0, 1e-9);
}

TEST_F(FilterDesignTest, PhasezUnwrappedIsContinuous)
{
    // After unwrap, no consecutive jump may exceed pi.
    eval("[phi, W] = phasez([0 0 0 0 0 0 0 0 1], [1], 128);");
    for (int i = 2; i <= 128; ++i) {
        double d = evalScalar("phi(" + std::to_string(i) + ") - phi("
                              + std::to_string(i - 1) + ");");
        EXPECT_LT(std::abs(d), M_PI);
    }
}

// ============================================================
// grpdelay
// ============================================================

TEST_F(FilterDesignTest, GrpdelayPassthroughIsZero)
{
    eval("[gd, W] = grpdelay([1], [1], 64);");
    for (int i = 1; i <= 64; ++i) {
        std::string g = "gd(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(g), 0.0, 1e-9);
    }
}

TEST_F(FilterDesignTest, GrpdelayPureDelayIsConstant)
{
    // 3-sample delay → group delay = 3 samples everywhere.
    eval("[gd, W] = grpdelay([0 0 0 1], [1], 128);");
    for (int i = 1; i <= 128; ++i) {
        double v = evalScalar("gd(" + std::to_string(i) + ");");
        EXPECT_NEAR(v, 3.0, 1e-6);
    }
}

TEST_F(FilterDesignTest, GrpdelayMatchesNegativeDerivativeOfPhasez)
{
    // Sanity: numerical -dphi/dw at an interior point matches gd.
    eval("[phi, W] = phasez([1 -0.4 0.2], [1 0.3], 128);"
         "[gd,  W2] = grpdelay([1 -0.4 0.2], [1 0.3], 128);"
         "i = 64;"
         "expected = -(phi(i+1) - phi(i-1)) / (W(i+1) - W(i-1));");
    double expected = evalScalar("expected");
    double actual   = evalScalar("gd(64)");
    EXPECT_NEAR(actual, expected, 1e-12);
}

TEST_F(FilterDesignTest, GrpdelayShape)
{
    eval("[gd, W] = grpdelay([1 0.5], [1], 64);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(gd)"), 64.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(W)"),  64.0);
}
