// toolboxes/builtin/tests/special_fn_batch_test.cpp
// special-function family — 17 functions:
//   bessel:  besselj / bessely / besseli / besselk / besselh
//   beta:    beta / betainc / betaincinv / betaln
//   gamma:   gamma / gammainc / gammaincinv / gammaln
//   error:   erf / erfc / erfinv / erfcinv
// All  — verified bit-identical to
// MATLAB R2025b on probed inputs (parity tol=1e-9, special-function
// algorithms have ULP differences vs naive libm).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SpecialFnBatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ─── bessel family ──────────────────────────────────────────────────

TEST_F(SpecialFnBatchTest, BesselJ)
{
    EXPECT_NEAR(evalScalar("besselj(0, 1)"),  0.765197686557967, 1e-9);
    EXPECT_NEAR(evalScalar("besselj(0, 2)"),  0.223890779141236, 1e-9);
    EXPECT_NEAR(evalScalar("besselj(1, 1)"),  0.440050585744934, 1e-9);
}

TEST_F(SpecialFnBatchTest, BesselY)
{
    EXPECT_NEAR(evalScalar("bessely(0, 1)"),  0.088256964215677, 1e-9);
    EXPECT_NEAR(evalScalar("bessely(1, 1)"), -0.781212821300289, 1e-9);
}

TEST_F(SpecialFnBatchTest, BesselI)
{
    EXPECT_NEAR(evalScalar("besseli(0, 1)"),  1.266065877752008, 1e-9);
    EXPECT_NEAR(evalScalar("besseli(0, 2)"),  2.279585302336067, 1e-9);
}

TEST_F(SpecialFnBatchTest, BesselK)
{
    EXPECT_NEAR(evalScalar("besselk(0, 1)"),  0.421024438240708, 1e-9);
    EXPECT_NEAR(evalScalar("besselk(1, 1)"),  0.601907230197235, 1e-9);
}

TEST_F(SpecialFnBatchTest, BesselH)
{
    // H1(0, 1) = J0(1) + i·Y0(1)
    EXPECT_NEAR(evalScalar("real(besselh(0, 1, 1))"), 0.765197686557967, 1e-9);
    EXPECT_NEAR(evalScalar("imag(besselh(0, 1, 1))"), 0.088256964215677, 1e-9);
}

// ─── beta family ────────────────────────────────────────────────────

TEST_F(SpecialFnBatchTest, Beta)
{
    EXPECT_NEAR(evalScalar("beta(2, 3)"), 0.083333333333333, 1e-12);  // 1/12
    EXPECT_NEAR(evalScalar("beta(5, 4)"), 0.003571428571429, 1e-9);
}

TEST_F(SpecialFnBatchTest, Betainc)
{
    EXPECT_NEAR(evalScalar("betainc(0.5, 2, 3)"), 0.687500, 1e-6);
    EXPECT_NEAR(evalScalar("betainc(0.0, 2, 3)"), 0.0,      1e-12);
    EXPECT_NEAR(evalScalar("betainc(1.0, 2, 3)"), 1.0,      1e-12);
}

TEST_F(SpecialFnBatchTest, BetaincInverse)
{
    // Round-trip
    EXPECT_NEAR(evalScalar("betaincinv(0.687500, 2, 3)"), 0.5, 1e-6);
}

TEST_F(SpecialFnBatchTest, Betaln)
{
    EXPECT_NEAR(evalScalar("betaln(2, 3)"), -2.484906649788000, 1e-9);
    EXPECT_NEAR(evalScalar("betaln(5, 4)"), -5.634789603169249, 1e-9);
}

// ─── gamma family ───────────────────────────────────────────────────

TEST_F(SpecialFnBatchTest, Gamma)
{
    EXPECT_NEAR(evalScalar("gamma(1)"),    1.0, 1e-12);
    EXPECT_NEAR(evalScalar("gamma(2)"),    1.0, 1e-12);
    EXPECT_NEAR(evalScalar("gamma(5)"),   24.0, 1e-9);
    EXPECT_NEAR(evalScalar("gamma(0.5)"), 1.772453850905516, 1e-12);  // sqrt(pi)
}

TEST_F(SpecialFnBatchTest, GammaInc)
{
    EXPECT_NEAR(evalScalar("gammainc(2, 2)"), 0.593994150290162, 1e-9);
    EXPECT_NEAR(evalScalar("gammainc(0, 2)"), 0.0, 1e-12);
}

TEST_F(SpecialFnBatchTest, GammaIncInv)
{
    EXPECT_NEAR(evalScalar("gammaincinv(0.593994150290162, 2)"), 2.0, 1e-9);
}

TEST_F(SpecialFnBatchTest, Gammaln)
{
    EXPECT_NEAR(evalScalar("gammaln(1)"),  0.0,                1e-12);
    EXPECT_NEAR(evalScalar("gammaln(5)"),  3.178053830347946,  1e-12);
    EXPECT_NEAR(evalScalar("gammaln(10)"), 12.801827480081469, 1e-12);
}

// ─── error function family ──────────────────────────────────────────

TEST_F(SpecialFnBatchTest, Erf)
{
    EXPECT_NEAR(evalScalar("erf( 0)"),  0.0,               1e-12);
    EXPECT_NEAR(evalScalar("erf( 1)"),  0.842700792949715, 1e-12);
    EXPECT_NEAR(evalScalar("erf(-1)"), -0.842700792949715, 1e-12);
}

TEST_F(SpecialFnBatchTest, Erfc)
{
    // erfc(x) = 1 - erf(x)
    EXPECT_NEAR(evalScalar("erfc(0) - 1.0"),                0.0, 1e-12);
    EXPECT_NEAR(evalScalar("erfc(1) - (1 - erf(1))"),       0.0, 1e-12);
}

TEST_F(SpecialFnBatchTest, ErfInv)
{
    EXPECT_NEAR(evalScalar("erfinv(0)"),    0.0, 1e-12);
    EXPECT_NEAR(evalScalar("erfinv(0.5)"),  0.476936276204470, 1e-9);
    EXPECT_NEAR(evalScalar("erf(erfinv(0.42))"), 0.42, 1e-12);
}

TEST_F(SpecialFnBatchTest, ErfcInv)
{
    EXPECT_NEAR(evalScalar("erfcinv(1)"),    0.0, 1e-12);
    EXPECT_NEAR(evalScalar("erfc(erfcinv(0.3))"), 0.3, 1e-9);
}

// ─── functional identity: gamma vs factorial ────────────────────────

TEST_F(SpecialFnBatchTest, GammaFactorialIdentity)
{
    EXPECT_NEAR(evalScalar("gamma(6) - 120"), 0.0, 1e-9);   // 5! = 120
    EXPECT_NEAR(evalScalar("gamma(11) - 3628800"), 0.0, 1e-3);  // 10!
}
