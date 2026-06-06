// libs/stats/tests/ansaribradley_test.cpp
//
// Regression guard for ansaribradley — production-grade verification
// against MATLAB R2025b reference values on deterministic data.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class AnsariBradleyTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Bit-exact MATLAB reference values ───────────────────────────────

TEST_F(AnsariBradleyTest, EqualDispersionInterleaved)
{
    // a = odd 1..19, b = even 2..20 — perfectly interleaved.
    // W = 55, Wstar = 0, p = 1.0 (asymptotic).
    eval(R"(
        a = [1 3 5 7 9 11 13 15 17 19]';
        b = [2 4 6 8 10 12 14 16 18 20]';
        [h, p, stats] = ansaribradley(a, b);
    )");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 0);
    EXPECT_NEAR(evalScalar("p"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("stats.W"), 55.0, 1e-12);
    EXPECT_NEAR(evalScalar("stats.Wstar"), 0.0, 1e-12);
}

TEST_F(AnsariBradleyTest, BSpreadWiderRejects)
{
    // a tightly clustered, b widely spread; W small (a in middle).
    // exact path (min(m,n)=5 ≤ 10).
    eval(R"(
        a = [-1 -0.5 0 0.5 1]';
        b = [-5 -3 -1 1 3 5]';
        [h, p, stats] = ansaribradley(a, b);
    )");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 1);
    EXPECT_NEAR(evalScalar("p"),     0.0259740260, 1e-9);
    EXPECT_NEAR(evalScalar("stats.W"),     23.0,    1e-12);
    EXPECT_NEAR(evalScalar("stats.Wstar"),  2.437399, 1e-5);
}

TEST_F(AnsariBradleyTest, SmallSampleExactBitExact)
{
    // m=4, n=3 → C(7,4)=35 permutations, exact distribution.
    eval(R"(
        a = [1 2 3 4]';
        b = [5 6 7]';
        [h, p, stats] = ansaribradley(a, b);
    )");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 0);
    EXPECT_NEAR(evalScalar("p"), 6.0 / 7.0, 1e-12);
    EXPECT_NEAR(evalScalar("stats.W"), 10.0, 1e-12);
}

TEST_F(AnsariBradleyTest, TiedSamplesCorrectExactP)
{
    // Ties: midrank denominators {1, 2, 3} → LCM=6 needed for
    // integer-DP. MATLAB: p = 0.8809523810.
    eval(R"(
        a = [1 2 2 3 4]';
        b = [2 3 3 4 5]';
        [h, p, stats] = ansaribradley(a, b);
    )");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 0);
    EXPECT_NEAR(evalScalar("p"),               0.8809523810, 1e-9);
    EXPECT_NEAR(evalScalar("stats.W"),         14.16666666666667, 1e-9);
    EXPECT_NEAR(evalScalar("stats.Wstar"),    -0.385376, 1e-5);
}

TEST_F(AnsariBradleyTest, LargeAsymptoticBitExact)
{
    // linspace data (m=20, n=25 — asymptotic path).
    eval(R"(
        a = linspace(-2, 2, 20)';
        b = linspace(-5, 5, 25)';
        [h, p, stats] = ansaribradley(a, b);
    )");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 1);
    EXPECT_NEAR(evalScalar("p"),              0.0006293629, 1e-9);
    EXPECT_NEAR(evalScalar("stats.W"),       310.0,         1e-12);
    EXPECT_NEAR(evalScalar("stats.Wstar"),    3.418634,     1e-5);
}

// ── Tail convention (inverted: 'right' = x more dispersed) ──────────

TEST_F(AnsariBradleyTest, RightTailInvertedConvention)
{
    // For Ansari-Bradley, 'right' tests dispersion(x) > dispersion(y).
    // Larger dispersion → smaller W → p = P(W ≤ obs).
    // From the LargeAsymptoticBitExact data: Wstar = 3.418 → P(W ≤ obs)
    // is huge (0.9997).
    eval(R"(
        a = linspace(-2, 2, 20)';
        b = linspace(-5, 5, 25)';
        [h, p, stats] = ansaribradley(a, b, 'Tail', 'right');
    )");
    EXPECT_NEAR(evalScalar("p"), 0.9996853186, 1e-9);
}

TEST_F(AnsariBradleyTest, LeftTailInvertedConvention)
{
    // 'left' = dispersion(x) < dispersion(y), should give small p when
    // Wstar > 0 (x near centre).
    eval(R"(
        a = linspace(-2, 2, 20)';
        b = linspace(-5, 5, 25)';
        [h, p, stats] = ansaribradley(a, b, 'Tail', 'left');
    )");
    EXPECT_NEAR(evalScalar("p"), 0.0003146814, 1e-9);
}

// ── Edge cases ──────────────────────────────────────────────────────

TEST_F(AnsariBradleyTest, EmptySampleHandled)
{
    eval("[h, p, stats] = ansaribradley([], [1 2 3]);");
    EXPECT_EQ(static_cast<int>(evalScalar("h")), 0);
    EXPECT_NEAR(evalScalar("p"), 1.0, 1e-12);
}

TEST_F(AnsariBradleyTest, AlphaArgument)
{
    // Tight clusters in a, wide b — p ≈ 0.026; should reject at α=0.05
    // but not at α=0.01.
    eval(R"(
        a = [-1 -0.5 0 0.5 1]';
        b = [-5 -3 -1 1 3 5]';
        [h1, p1] = ansaribradley(a, b, 0.05);
        [h2, p2] = ansaribradley(a, b, 0.01);
    )");
    EXPECT_EQ(static_cast<int>(evalScalar("h1")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("h2")), 0);
}

TEST_F(AnsariBradleyTest, NaNsIgnored)
{
    // NaN entries should be dropped from both samples.
    eval(R"(
        a = [1 2 NaN 3 4]';
        b = [5 6 7 NaN]';
        [h, p, stats] = ansaribradley(a, b);
        % Should equal ansaribradley([1 2 3 4]', [5 6 7]') i.e. W = 10.
    )");
    EXPECT_NEAR(evalScalar("stats.W"), 10.0, 1e-12);
}
