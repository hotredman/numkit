// libs/signal/tests/multirate_extras_test.cpp
//
// Tests for F1: upfirdn / interp / intfilt / fftfilt.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class MultirateExtrasTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── intfilt ───────────────────────────────────────────────────────────
TEST_F(MultirateExtrasTest, IntfiltLength)
{
    // MATLAB convention: length = 2 * R * L - 1.
    eval("h = intfilt(4, 3);");           // 2*3*4 - 1 = 23
    EXPECT_EQ(eval("h").numel(), 23u);
}

TEST_F(MultirateExtrasTest, IntfiltSymmetric)
{
    eval("h = intfilt(3, 4);");
    const size_t L = eval("h").numel();
    for (size_t i = 1; i <= L / 2; ++i) {
        const double l = evalScalar("h(" + std::to_string(i) + ")");
        const double r = evalScalar("h(" + std::to_string(L + 1 - i) + ")");
        EXPECT_NEAR(l, r, 1e-12);
    }
}

// intfilt's bandlimited form is firls(2*R*L-2, F*2, M) (MATLAB R2025b),
// not a windowed sinc. Coefficient VALUES pinned to MATLAB.
TEST_F(MultirateExtrasTest, IntfiltMatchesFirls)
{
    eval("b = intfilt(4, 2, 0.5);");          // length 15
    EXPECT_EQ(eval("b").numel(), 15u);
    EXPECT_NEAR(evalScalar("b(1)"), -0.0582705792, 1e-9);
    EXPECT_NEAR(evalScalar("b(8)"),  1.0,          1e-9);  // centre tap
    EXPECT_NEAR(evalScalar("sum(b)"), 3.9678094037, 1e-9);

    eval("c = intfilt(3, 4, 0.6);");
    EXPECT_NEAR(evalScalar("c(1)"),  -0.0089374149, 1e-9);
    EXPECT_NEAR(evalScalar("c(12)"),  1.0,          1e-9);

    // freqmult == 1 takes the [R R 0 0] band special case.
    eval("d = intfilt(2, 2, 1);");            // length 7
    EXPECT_EQ(eval("d").numel(), 7u);
    EXPECT_NEAR(evalScalar("d(1)"), -0.2122065908, 1e-9);
    EXPECT_NEAR(evalScalar("d(4)"),  1.0,          1e-9);
}

// ── upfirdn ───────────────────────────────────────────────────────────
TEST_F(MultirateExtrasTest, UpfirdnIdentityWithDelta)
{
    // p=1, q=1, h=delta → output equals input.
    eval("y = upfirdn([1 2 3 4], [1], 1, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 4.0);
}

TEST_F(MultirateExtrasTest, UpfirdnUpsampleFactor2)
{
    // p=2 with h=[1] → zero-stuff.
    eval("y = upfirdn([1 2 3], [1], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 3.0);
}

// ── interp ────────────────────────────────────────────────────────────
TEST_F(MultirateExtrasTest, InterpExpandsByFactor)
{
    eval("x = sin(2*pi*0.05*(0:31));");   // length 32
    eval("y = interp(x, 4);");
    EXPECT_EQ(eval("y").numel(), 32u * 4u);
}

TEST_F(MultirateExtrasTest, InterpPreservesLowFrequency)
{
    // Pure low-frequency tone — interp should reproduce the same tone.
    eval("t = 0:127; x = sin(2*pi*0.02*t);");
    eval("y = interp(x, 2, 8);");
    // Centre block of y should still be a sine wave with rms ≈ 1/sqrt(2).
    eval("yc = y(50:200);");
    EXPECT_NEAR(evalScalar("rms(yc)"), 1.0 / std::sqrt(2.0), 0.05);
}

// ── fftfilt ───────────────────────────────────────────────────────────
TEST_F(MultirateExtrasTest, FftfiltMatchesFilterShortInput)
{
    eval("b = [0.25 0.5 0.25];");
    eval("x = randn(1, 128);");
    eval("y_dir = filter(b, 1, x);");
    eval("y_fft = fftfilt(b, x);");
    // Compare to direct filter().
    for (int i = 1; i <= 128; ++i) {
        EXPECT_NEAR(evalScalar("y_dir(" + std::to_string(i) + ")"),
                    evalScalar("y_fft(" + std::to_string(i) + ")"), 1e-9);
    }
}

TEST_F(MultirateExtrasTest, FftfiltLengthMatchesInput)
{
    eval("y = fftfilt([1 1 1], 1:50);");
    EXPECT_EQ(eval("y").numel(), 50u);
}
