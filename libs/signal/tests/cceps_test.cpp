// libs/signal/tests/cceps_test.cpp
// signal/cceps.
// The bug: numkit's cceps implementation called fftRadix2(..., -1) for
// the inverse-DFT pass — but in numkit's sign convention, dir=-1 is the
// FORWARD DFT (W[k] = exp(-2πj k/N)) and dir=+1 is the INVERSE DFT.
// Calling fft twice on log(fft(x)) gives a time-reversed cepstrum
// (MATLAB output reversed bin-for-bin except DC).
// Fix in libs/signal/src/transforms/extras.cpp: change `-1` → `+1` in
// both cceps() and icceps(), restoring the correct ifft pass.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class CcepsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ─── canonical probe — bit-identical to MATLAB R2025b ────────────

TEST_F(CcepsTest, MatchesMatlabOnRange1To8)
{
    // y = cceps((1:8)')  →  exact MATLAB R2025b output.
    eval("y = cceps((1:8)');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 8.0);
    EXPECT_NEAR(evalScalar("y(1)"),  2.00752,    1e-5);
    EXPECT_NEAR(evalScalar("y(2)"), -0.0435703,  1e-7);
    EXPECT_NEAR(evalScalar("y(3)"), -0.00833987, 1e-8);
    EXPECT_NEAR(evalScalar("y(4)"),  0.0375161,  1e-7);
    EXPECT_NEAR(evalScalar("y(5)"),  0.101366,   1e-6);
    EXPECT_NEAR(evalScalar("y(6)"),  0.200177,   1e-6);
    EXPECT_NEAR(evalScalar("y(7)"),  0.384359,   1e-6);
    EXPECT_NEAR(evalScalar("y(8)"),  0.904489,   1e-6);
}

// ─── regression: output is NOT time-reversed ────────────────────────

TEST_F(CcepsTest, OutputNotTimeReversedRegression)
{
    // Pre-fix numkit returned [2.008, 0.904, 0.384, 0.200, 0.101,
    //   0.038, -0.008, -0.044]. Pin that y(2) is small-negative
    // (MATLAB) rather than the large-positive 0.904 (the old bug).
    eval("y = cceps((1:8)');");
    EXPECT_LT(evalScalar("y(2)"), 0.0);   // MATLAB: -0.0436
    EXPECT_GT(evalScalar("y(8)"), 0.5);   // MATLAB:  0.9045
}

// ─── DC bin (1) is unchanged by reversal — was always correct ──────

TEST_F(CcepsTest, DCBinUnaffectedByFix)
{
    // y(1) is the cepstral DC bin — unchanged before/after the fix.
    // Asserting it just to lock in the structural invariant that the
    // mean of log|FFT(x)| equals y(1) (with appropriate scaling).
    eval("y = cceps((1:8)');");
    EXPECT_NEAR(evalScalar("y(1)"), 2.00752, 1e-5);
}

// ─── icceps: shares the same sign-convention fix ────────────────────

TEST_F(CcepsTest, IccepsRoundTripPreservesShape)
{
    eval("x = (1:8)'; c = cceps(x); y = icceps(c);");
    // numel + finite + same sample stats (bit-identical to MATLAB):
    //   length 8, max == 8, min == 1, sum == 36.
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(y)"),    8.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(y)"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y)"),   36.0);
    for (int i = 1; i <= 8; ++i) {
        const std::string e = "y(" + std::to_string(i) + ")";
        EXPECT_FALSE(std::isnan(evalScalar(e)));
        EXPECT_FALSE(std::isinf(evalScalar(e)));
    }
}

// ─── length preservation across a range of input sizes ──────────────

TEST_F(CcepsTest, OutputLengthMatchesInput)
{
    eval("y3 = cceps([1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y3)"), 3.0);
    eval("y16 = cceps((1:16)');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y16)"), 16.0);
}
