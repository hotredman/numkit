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
    // Γ(-n) is a pole — MATLAB returns +Inf at every non-positive integer
    // (and at -Inf), not NaN. bugs/builtin/gamma-negative-integer-poles.md.
    EXPECT_TRUE(std::isinf(evalScalar("gamma(-1);")));
    EXPECT_GT(evalScalar("gamma(-1);"), 0.0);   // +Inf, not -Inf
    EXPECT_TRUE(std::isinf(evalScalar("gamma(-2);")));
    EXPECT_GT(evalScalar("gamma(-2);"), 0.0);
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

// ── Pack 36: airy ─────────────────────────────────────────────────
// Reference values are MATLAB R2025b's airy(k, x) probed 2026-05-03.
TEST_P(SpecialFuncsTest, AiryAtZero)
{
    EXPECT_NEAR(evalScalar("airy(0, 0);"),  0.35502805388781722, 1e-14);
    EXPECT_NEAR(evalScalar("airy(1, 0);"), -0.25881940379280682, 1e-14);
    EXPECT_NEAR(evalScalar("airy(2, 0);"),  0.61492662744600068, 1e-14);
    EXPECT_NEAR(evalScalar("airy(3, 0);"),  0.44828835735382638, 1e-14);
}

TEST_P(SpecialFuncsTest, AiryNegativeArgument)
{
    EXPECT_NEAR(evalScalar("airy(0, -1);"),  0.53556088329235219, 1e-13);
    EXPECT_NEAR(evalScalar("airy(1, -1);"), -0.01016056711664515, 1e-13);
    EXPECT_NEAR(evalScalar("airy(2, -1);"),  0.10399738949694459, 1e-13);
    EXPECT_NEAR(evalScalar("airy(3, -1);"),  0.59237562642279229, 1e-13);
}

TEST_P(SpecialFuncsTest, AiryPositiveArgument)
{
    EXPECT_NEAR(evalScalar("airy(0, 1);"),   0.13529241631288147, 1e-14);
    EXPECT_NEAR(evalScalar("airy(1, 1);"),  -0.15914744129679323, 1e-14);
    EXPECT_NEAR(evalScalar("airy(2, 1);"),   1.20742359495287133, 1e-14);
    EXPECT_NEAR(evalScalar("airy(3, 1);"),   0.93243593339277542, 1e-14);
}

TEST_P(SpecialFuncsTest, AiryLargeArgument)
{
    // Bi(3) ≈ 14.0373, exponential-ish growth.
    EXPECT_NEAR(evalScalar("airy(0, 3);"), 0.00659113935746072, 1e-14);
    EXPECT_NEAR(evalScalar("airy(2, 3);"), 14.037328963730223,  1e-12);
}

TEST_P(SpecialFuncsTest, AiryDefaultIsAi)
{
    // airy(x) with one arg should equal airy(0, x).
    EXPECT_NEAR(evalScalar("airy(1.5);"), evalScalar("airy(0, 1.5);"), 1e-15);
}

TEST_P(SpecialFuncsTest, AiryBadKThrows)
{
    EXPECT_THROW(eval("airy(5, 1);"), std::exception);
}

// ── Pack 36: gammaincinv / betaincinv / ellipj ───────────────────────
TEST_P(SpecialFuncsTest, GammaincinvBoundary)
{
    EXPECT_DOUBLE_EQ(evalScalar("gammaincinv(0, 2);"), 0.0);
    EXPECT_TRUE(std::isinf(evalScalar("gammaincinv(1, 2);")));
}

TEST_P(SpecialFuncsTest, GammaincinvRoundtrip)
{
    // gammainc(gammaincinv(P, a), a) ≈ P for sample (P, a).
    eval("vals = [0.1 0.3 0.5 0.7 0.9; 0.5 1 2 5 10];");
    eval("err = 0;");
    for (int i = 1; i <= 5; ++i) {
        const std::string code =
            "p = vals(1, " + std::to_string(i) + "); "
            "a = vals(2, " + std::to_string(i) + "); "
            "r = gammainc(gammaincinv(p, a), a); "
            "err = max(err, abs(r - p));";
        eval(code);
    }
    EXPECT_LT(evalScalar("err;"), 1e-12);
}

TEST_P(SpecialFuncsTest, GammaincinvKnownHalfA1)
{
    // gammaincinv(0.5, 1) = -log(0.5) = ln(2).
    EXPECT_NEAR(evalScalar("gammaincinv(0.5, 1);"), 0.6931471805599453, 1e-12);
}

TEST_P(SpecialFuncsTest, BetaincinvBoundary)
{
    EXPECT_DOUBLE_EQ(evalScalar("betaincinv(0, 2, 3);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("betaincinv(1, 2, 3);"), 1.0);
}

TEST_P(SpecialFuncsTest, BetaincinvRoundtrip)
{
    eval("err = 0;");
    eval("aa = [1 2 3 5 7]; bb = [2 5 1 3 4]; pp = [0.2 0.4 0.5 0.7 0.9];");
    for (int i = 1; i <= 5; ++i) {
        const std::string code =
            "a = aa(" + std::to_string(i) + "); "
            "b = bb(" + std::to_string(i) + "); "
            "p = pp(" + std::to_string(i) + "); "
            "r = betainc(betaincinv(p, a, b), a, b); "
            "err = max(err, abs(r - p));";
        eval(code);
    }
    EXPECT_LT(evalScalar("err;"), 1e-11);
}

TEST_P(SpecialFuncsTest, EllipjAtZeroM)
{
    // m=0 → sn=sin, cn=cos, dn=1.
    eval("[s, c, d] = ellipj(1.0, 0);");
    EXPECT_NEAR(evalScalar("s;"), std::sin(1.0), 1e-15);
    EXPECT_NEAR(evalScalar("c;"), std::cos(1.0), 1e-15);
    EXPECT_DOUBLE_EQ(evalScalar("d;"), 1.0);
}

TEST_P(SpecialFuncsTest, EllipjAtOneM)
{
    // m=1 → sn=tanh, cn=dn=sech.
    eval("[s, c, d] = ellipj(1.0, 1);");
    EXPECT_NEAR(evalScalar("s;"), std::tanh(1.0), 1e-15);
    EXPECT_NEAR(evalScalar("c;"), 1.0/std::cosh(1.0), 1e-15);
    EXPECT_NEAR(evalScalar("d;"), 1.0/std::cosh(1.0), 1e-15);
}

TEST_P(SpecialFuncsTest, EllipjPythagoreanIdentities)
{
    // Identities: sn²+cn²=1 ; dn²+m·sn²=1.
    eval("[s, c, d] = ellipj(1.5, 0.5);");
    EXPECT_NEAR(evalScalar("s*s + c*c;"), 1.0, 1e-14);
    EXPECT_NEAR(evalScalar("d*d + 0.5*s*s;"), 1.0, 1e-14);
}

TEST_P(SpecialFuncsTest, EllipjMatlabReference)
{
    // Cross-check against probed MATLAB R2025b values.
    eval("[s, c, d] = ellipj(1.5, 0.5);");
    EXPECT_NEAR(evalScalar("s;"), 0.968176015675691, 1e-13);
    EXPECT_NEAR(evalScalar("c;"), 0.250270259260551, 1e-13);
    EXPECT_NEAR(evalScalar("d;"), 0.728915359513827, 1e-13);
}
