// libs/signal/tests/kaiserord_test.cpp
//
// Regression guard for kaiserord (Phase 4.5).
// Bit-equal MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class KaiserordTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── lowpass ────────────────────────────────────────────────────────────
TEST_F(KaiserordTest, LowpassFromHelpExample)
{
    // MATLAB help example: 1500/2000 transition, 0.01/0.1 dev, fs=8000.
    eval("[n, Wn, bta, ft] = kaiserord([1500 2000], [1 0], [0.01 0.1], 8000);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 36);
    EXPECT_NEAR(evalScalar("Wn"), 0.4375, 1e-9);
    EXPECT_NEAR(evalScalar("bta"), 3.395321, 1e-5);
    EXPECT_DOUBLE_EQ(evalScalar("double(ft(1))"), 108.0);  // 'l'
}

// ── highpass ───────────────────────────────────────────────────────────
TEST_F(KaiserordTest, HighpassMagsZeroFirst)
{
    eval("[n, Wn, bta, ft] = kaiserord([800 1000], [0 1], [0.01 0.05], 4000);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 46);
    EXPECT_NEAR(evalScalar("Wn"), 0.45, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("double(ft(1))"), 104.0);  // 'h'
}

// ── bandpass / multiband ───────────────────────────────────────────────
TEST_F(KaiserordTest, BandpassMultiband)
{
    eval("[n, Wn, bta, ft] = kaiserord([500 1000 2000 2500], [0 1 0], "
         "                              [0.05 0.01 0.05], 8000);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 36);
    EXPECT_NEAR(evalScalar("Wn(1)"), 0.1875, 1e-9);
    EXPECT_NEAR(evalScalar("Wn(2)"), 0.5625, 1e-9);
    // ftype = "DC-0" → first char 'D' = 68
    EXPECT_DOUBLE_EQ(evalScalar("double(ft(1))"), 68.0);
}

// ── default fs=2 (normalized) ──────────────────────────────────────────
TEST_F(KaiserordTest, DefaultFs2Normalized)
{
    // F values directly in normalized rad/π (since fs=2 means F/2 = F * π/π).
    eval("[n, ~, bta, ~] = kaiserord([0.4 0.5], [1 0], [0.01 0.1]);");
    EXPECT_GT(static_cast<int>(evalScalar("n")), 0);
    EXPECT_GT(evalScalar("bta"), 0.0);
}

// ── Kaiser β piecewise ─────────────────────────────────────────────────
TEST_F(KaiserordTest, BetaPiecewiseAtBoundaries)
{
    // Smaller dev → larger atten → β formula switches.
    eval("[~, ~, b1, ~] = kaiserord([0.4 0.5], [1 0], [0.0001 0.0001]);");  // ~80 dB
    EXPECT_GT(evalScalar("b1"), 7.0);  // β > 7 for atten>50
}
