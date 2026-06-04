// libs/builtin/tests/power_complex_test.cpp
//
// Regression guard: a negative real base raised to a non-integer exponent
// is complex (MATLAB R2025b), e.g. (-8)^(1/3) == 1+1.7321i. Integer
// exponents and positive bases stay real. Two-arg array .^ promotes
// per-pair (only the elements that are neg-base + non-integer-exp). Runs
// on BOTH engines and covers the `^` / `.^` operators and power() builtin.
//
// Bug history: the whole power subsystem used std::pow on reals and
// returned NaN. Fixed in power()/elementPower() (binary_ops.cpp) with the
// fast paths (VM POW/POW_SS/EPOW, tree-walker .^, compiler const-fold)
// bailing to power() for the negative-base/non-integer-exp case.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class PowerComplexTest : public DualEngineTest {};

static constexpr double kSqrt3 = 1.7320508075688772; // imag of (-8)^(1/3) = 2*sin(pi/3)
static constexpr double kSqrt8 = 2.8284271247461903; // sqrt(8)

TEST_P(PowerComplexTest, NegativeBaseFractionalExpScalar)
{
    EXPECT_FALSE(evalBool("isreal((-8)^(1/3))"));
    EXPECT_NEAR(evalScalar("real((-8)^(1/3))"), 1.0,    1e-12);
    EXPECT_NEAR(evalScalar("imag((-8)^(1/3))"), kSqrt3, 1e-12);
    EXPECT_NEAR(evalScalar("real((-2)^0.5)"),   0.0,    1e-12); // ~1e-16
    EXPECT_NEAR(evalScalar("imag((-2)^0.5)"),   1.4142135623730951, 1e-12);
    // power() builtin and the .^ operator agree
    EXPECT_NEAR(evalScalar("imag(power(-8, 1/3))"), kSqrt3, 1e-12);
    EXPECT_NEAR(evalScalar("imag((-8).^(1/3))"),    kSqrt3, 1e-12);
    // runtime variable (not a literal) exercises the non-fold path
    eval("t = -8;");
    EXPECT_NEAR(evalScalar("imag(t^(1/3))"), kSqrt3, 1e-12);
}

TEST_P(PowerComplexTest, IntegerExpAndPositiveBaseStayReal)
{
    EXPECT_TRUE(evalBool("isreal((-8)^2)"));
    EXPECT_DOUBLE_EQ(evalScalar("(-8)^2"),  64.0);
    EXPECT_DOUBLE_EQ(evalScalar("(-8)^3"), -512.0);
    EXPECT_TRUE(evalBool("isreal(8^(1/3))"));
    EXPECT_DOUBLE_EQ(evalScalar("8^(1/3)"), 2.0);
    EXPECT_TRUE(evalBool("isreal(2^10)"));
    EXPECT_DOUBLE_EQ(evalScalar("2^10"), 1024.0);
}

TEST_P(PowerComplexTest, ArrayPromotesPerPair)
{
    // Neither pair is (neg base, non-integer exp) -> stays real.
    EXPECT_TRUE(evalBool("isreal([-8 8].^[2 0.5])"));
    eval("r = [-8 8].^[2 0.5];");
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 64.0);
    EXPECT_NEAR(evalScalar("r(2)"), kSqrt8, 1e-12);
    // First pair (-8)^0.5 IS complex -> whole array complex.
    EXPECT_FALSE(evalBool("isreal([-8 8].^[0.5 2])"));
    eval("c = [-8 8].^[0.5 2];");
    EXPECT_NEAR(evalScalar("imag(c(1))"), kSqrt8, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("real(c(2))"), 64.0);
    // array .^ scalar exponent
    eval("p = power([-8 8 -27], 1/3);");
    EXPECT_FALSE(evalBool("isreal(p)"));
    EXPECT_NEAR(evalScalar("imag(p(1))"), kSqrt3, 1e-12);          // (-8)^(1/3)
    EXPECT_NEAR(evalScalar("real(p(2))"), 2.0,    1e-12);          // 8^(1/3)
    EXPECT_NEAR(evalScalar("real(p(3))"), 1.5,    1e-12);          // (-27)^(1/3)=1.5+2.598i
    EXPECT_NEAR(evalScalar("imag(p(3))"), 2.598076211353316, 1e-12);
}

INSTANTIATE_DUAL(PowerComplexTest);
