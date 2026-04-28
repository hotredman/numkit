// libs/builtin/tests/special_funcs_test.cpp
//
// Special functions: gamma, gammaln, erf, erfc, erfinv.

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class SpecialFuncsTest : public DualEngineTest
{};

// ── gamma ───────────────────────────────────────────────────────

TEST_P(SpecialFuncsTest, GammaIntegerEqualsFactorial)
{
    // gamma(n+1) = n!
    EXPECT_NEAR(evalScalar("gamma(1);"),   1.0,    1e-12);
    EXPECT_NEAR(evalScalar("gamma(2);"),   1.0,    1e-12);
    EXPECT_NEAR(evalScalar("gamma(3);"),   2.0,    1e-12);
    EXPECT_NEAR(evalScalar("gamma(4);"),   6.0,    1e-12);
    EXPECT_NEAR(evalScalar("gamma(5);"),  24.0,    1e-12);
    EXPECT_NEAR(evalScalar("gamma(6);"), 120.0,    1e-12);
}

TEST_P(SpecialFuncsTest, GammaHalfIntegers)
{
    // gamma(1/2) = sqrt(pi); gamma(3/2) = sqrt(pi)/2.
    const double sp = std::sqrt(M_PI);
    EXPECT_NEAR(evalScalar("gamma(0.5);"), sp,        1e-12);
    EXPECT_NEAR(evalScalar("gamma(1.5);"), sp / 2.0,  1e-12);
    EXPECT_NEAR(evalScalar("gamma(2.5);"), 0.75 * sp, 1e-12);
}

TEST_P(SpecialFuncsTest, GammaPole)
{
    // Γ(0) is +Inf (pole).
    EXPECT_TRUE(std::isinf(evalScalar("gamma(0);")));
}

TEST_P(SpecialFuncsTest, GammaNegativeIntegerIsPole)
{
    // Γ(-1) is a pole — std::tgamma signals it as either NaN or ±Inf.
    // Both behaviours are defensible; the only invariant we check is
    // that the result is *not* a finite number.
    const double v = evalScalar("gamma(-1);");
    EXPECT_FALSE(std::isfinite(v));
}

TEST_P(SpecialFuncsTest, GammaArrayShape)
{
    eval("y = gamma([1 2 3 4]);");
    auto *y = getVarPtr("y");
    EXPECT_EQ(y->numel(), 4u);
    EXPECT_DOUBLE_EQ(y->doubleData()[0],   1.0);
    EXPECT_DOUBLE_EQ(y->doubleData()[3],   6.0);
}

// ── gammaln ─────────────────────────────────────────────────────

TEST_P(SpecialFuncsTest, GammalnEqualsLogOfGamma)
{
    // gammaln(x) = log(|gamma(x)|) for positive x.
    for (double x : {0.5, 1.0, 2.5, 10.0}) {
        const double code_g  = std::lgamma(x);
        EXPECT_NEAR(evalScalar("gammaln(" + std::to_string(x) + ");"),
                    code_g, 1e-12);
    }
}

TEST_P(SpecialFuncsTest, GammalnLargeArgumentNoOverflow)
{
    // gamma(200) overflows; gammaln(200) is finite.
    const double v = evalScalar("gammaln(200);");
    EXPECT_TRUE(std::isfinite(v));
    EXPECT_GT(v, 800.0);  // ≈ 857.93
}

// ── erf / erfc ─────────────────────────────────────────────────

TEST_P(SpecialFuncsTest, ErfKnownValues)
{
    EXPECT_NEAR(evalScalar("erf(0);"),  0.0,                1e-15);
    EXPECT_NEAR(evalScalar("erf(1);"),  0.8427007929497149, 1e-12);
    EXPECT_NEAR(evalScalar("erf(2);"),  0.9953222650189527, 1e-12);
    EXPECT_NEAR(evalScalar("erf(-1);"), -0.8427007929497149, 1e-12);
}

TEST_P(SpecialFuncsTest, ErfApproachesOne)
{
    EXPECT_NEAR(evalScalar("erf(10);"), 1.0, 1e-12);
}

TEST_P(SpecialFuncsTest, ErfcEqualsOneMinusErf)
{
    for (double x : {-2.0, -0.5, 0.0, 0.7, 3.0}) {
        const double code_e = 1.0 - std::erf(x);
        EXPECT_NEAR(evalScalar("erfc(" + std::to_string(x) + ");"),
                    code_e, 1e-12);
    }
}

TEST_P(SpecialFuncsTest, ErfcLargeArgumentRetainsAccuracy)
{
    // 1 - erf(5) ≈ 1.5e-12 — double subtraction would lose all bits.
    // erfc avoids that.
    const double v = evalScalar("erfc(5);");
    EXPECT_LT(v, 2e-12);
    EXPECT_GT(v, 1e-12);
}

// ── erfinv ─────────────────────────────────────────────────────

TEST_P(SpecialFuncsTest, ErfinvIsInverseOfErf)
{
    // erfinv(erf(x)) ≈ x.
    for (double x : {-1.5, -0.5, 0.0, 0.3, 1.7}) {
        const std::string code = "erfinv(erf(" + std::to_string(x) + "));";
        EXPECT_NEAR(evalScalar(code), x, 1e-12) << "at x=" << x;
    }
}

TEST_P(SpecialFuncsTest, ErfErfinvIsIdentity)
{
    // erf(erfinv(y)) ≈ y for y ∈ (-1, 1).
    for (double y : {-0.9, -0.4, 0.0, 0.2, 0.99}) {
        const std::string code = "erf(erfinv(" + std::to_string(y) + "));";
        EXPECT_NEAR(evalScalar(code), y, 1e-12) << "at y=" << y;
    }
}

TEST_P(SpecialFuncsTest, ErfinvBoundaryReturnsInf)
{
    EXPECT_TRUE(std::isinf(evalScalar("erfinv(1);")));
    EXPECT_TRUE(std::isinf(evalScalar("erfinv(-1);")));
}

TEST_P(SpecialFuncsTest, ErfinvOutsideRangeIsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("erfinv(2);")));
    EXPECT_TRUE(std::isnan(evalScalar("erfinv(-1.5);")));
}

TEST_P(SpecialFuncsTest, ErfinvVectorInput)
{
    eval("y = erfinv([0 0.5 -0.5]);");
    auto *y = getVarPtr("y");
    EXPECT_EQ(y->numel(), 3u);
    EXPECT_NEAR(y->doubleData()[0], 0.0, 1e-15);
    EXPECT_NEAR(y->doubleData()[1],  0.4769362762044698, 1e-12);
    EXPECT_NEAR(y->doubleData()[2], -0.4769362762044698, 1e-12);
}

INSTANTIATE_DUAL(SpecialFuncsTest);
