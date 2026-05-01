// libs/signal/tests/waveform_extras_test.cpp
//
// Tests for package A4 (waveform_generation extras):
//   square, sawtooth, sinc, gmonopuls, diric.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class WaveformExtrasTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        // `square` is intentionally NOT in compat (clashes with the user-fn
        // identifier in core tests under TW). Reach it via its full path.
        engine.eval("import compat.*;");
        engine.eval("import signal.waveform_generation.*;");
    }
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// Fully-qualified to bypass the missing compat alias for `square`.
static const char *SQ = "signal.waveform_generation.square";

// ── square ────────────────────────────────────────────────────────────
TEST_F(WaveformExtrasTest, SquareBasicHalfPeriod)
{
    // square(t) at t = 0.1 (just inside first half-period) → +1.
    EXPECT_NEAR(evalScalar(std::string(SQ) + "(0.1)"), 1.0, 1e-12);
    // At t just past π → -1.
    EXPECT_NEAR(evalScalar(std::string(SQ) + "(pi + 0.1)"), -1.0, 1e-12);
}

TEST_F(WaveformExtrasTest, SquarePeriodic)
{
    // square is 2π-periodic.
    EXPECT_NEAR(evalScalar(std::string(SQ) + "(0.5)"),
                evalScalar(std::string(SQ) + "(0.5 + 2*pi)"), 1e-12);
}

TEST_F(WaveformExtrasTest, SquareDuty25)
{
    // duty=25 → high for first 25% of period, then low.
    EXPECT_NEAR(evalScalar(std::string(SQ) + "(0.1, 25)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar(std::string(SQ) + "(2.0, 25)"), -1.0, 1e-12);  // 2.0/(2π) ≈ 0.318 > 0.25
}

// ── sawtooth ──────────────────────────────────────────────────────────
TEST_F(WaveformExtrasTest, SawtoothCanonicalRamp)
{
    // sawtooth(0) starts at -1. sawtooth(2π - ε) approaches +1. Exactly
    // at multiples of 2π it wraps to -1.
    EXPECT_NEAR(evalScalar("sawtooth(0)"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sawtooth(pi)"), 0.0, 1e-12);
}

TEST_F(WaveformExtrasTest, SawtoothWidthHalfIsTriangle)
{
    // width = 0.5 → triangle wave. Peak +1 at t = π.
    EXPECT_NEAR(evalScalar("sawtooth(pi, 0.5)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sawtooth(0, 0.5)"),  -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sawtooth(2*pi - 1e-9, 0.5)"), -1.0, 1e-6);
}

// ── sinc ──────────────────────────────────────────────────────────────
TEST_F(WaveformExtrasTest, SincZeroIsOne)
{
    EXPECT_NEAR(evalScalar("sinc(0)"), 1.0, 1e-12);
}

TEST_F(WaveformExtrasTest, SincIntegerZeros)
{
    for (int k = 1; k <= 5; ++k) {
        EXPECT_NEAR(evalScalar("sinc(" + std::to_string(k) + ")"), 0.0, 1e-12)
            << "sinc(" << k << ")";
    }
}

TEST_F(WaveformExtrasTest, SincVectorPreservesShape)
{
    eval("y = sinc(linspace(-2, 2, 5));");
    EXPECT_EQ(eval("y").numel(), 5u);
    EXPECT_NEAR(evalScalar("y(3)"), 1.0, 1e-12);   // centre = sinc(0)
}

// ── gmonopuls ─────────────────────────────────────────────────────────
TEST_F(WaveformExtrasTest, GmonopulsZeroAtT0)
{
    EXPECT_NEAR(evalScalar("gmonopuls(0, 1e3)"), 0.0, 1e-12);
}

TEST_F(WaveformExtrasTest, GmonopulsPeakAtTcAndUnitAmplitude)
{
    // Peak occurs at t = 1/(2π·fc) and equals +1.
    eval("fc = 1000; tc = 1/(2*pi*fc); y = gmonopuls(tc, fc);");
    EXPECT_NEAR(evalScalar("y"), 1.0, 1e-9);
}

TEST_F(WaveformExtrasTest, GmonopulsAntisymmetric)
{
    // Real-valued odd function: y(-t) = -y(t).
    eval("fc = 500; t = 0.0005;");
    EXPECT_NEAR(evalScalar("gmonopuls(t, fc) + gmonopuls(-t, fc)"), 0.0, 1e-12);
}

// ── diric ─────────────────────────────────────────────────────────────
TEST_F(WaveformExtrasTest, DiricAtZero)
{
    // diric(0, n) = 1.
    EXPECT_NEAR(evalScalar("diric(0, 5)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("diric(0, 10)"), 1.0, 1e-12);
}

TEST_F(WaveformExtrasTest, DiricAtTwoPi)
{
    // diric(2π, n) = (-1)^(n-1).
    EXPECT_NEAR(evalScalar("diric(2*pi, 5)"),  1.0, 1e-12);   // (-1)^4 = 1
    EXPECT_NEAR(evalScalar("diric(2*pi, 4)"), -1.0, 1e-12);   // (-1)^3 = -1
}

TEST_F(WaveformExtrasTest, DiricVectorShape)
{
    eval("y = diric(linspace(-pi, pi, 11), 7);");
    EXPECT_EQ(eval("y").numel(), 11u);
}

// Regression: bare `square` (no namespace prefix) must NOT resolve to the
// signal-toolbox builtin after `import compat.*`. Catches accidental
// re-introduction of the compat alias for `square`, which collides with
// user-defined `function y = square(x)` in core/builtin tests on TW.
TEST_F(WaveformExtrasTest, SquareNotAliasedIntoCompat)
{
    Engine fresh;
    fresh.eval("import compat.*;");
    bool threw = false;
    try {
        fresh.eval("y = square(0.1);");
    } catch (...) {
        threw = true;
    }
    EXPECT_TRUE(threw)
        << "`square` leaked into compat — would shadow user-fn in TW tests";
}
