// libs/builtin/tests/complex_promotion_test.cpp
//
// Regression guard: sqrt / log / log2 / log10 of a NEGATIVE real scalar
// must promote to a complex result (MATLAB R2025b parity), on BOTH the
// TreeWalker and the Bytecode VM.
//
// Bug history: the scalar fast paths (vm.cpp execCallBuiltin and
// tree_walker.cpp tryEvalFast) computed std::sqrt / std::log directly for
// a scalar double, returning real NaN for negative inputs and bypassing
// the Value wrapper's negative->complex branch. sqrt(-4) returned NaN
// instead of 0+2i. Fixed by bailing the fast path for v < 0 so the full
// builtin runs (and by giving log2/log10 the same negative->complex
// branch that sqrt/log already had). Expected values pinned from MATLAB.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class ComplexPromotionTest : public DualEngineTest {};

TEST_P(ComplexPromotionTest, SqrtNegativeScalar)
{
    EXPECT_FALSE(evalBool("isreal(sqrt(-4))"));
    EXPECT_DOUBLE_EQ(evalScalar("real(sqrt(-4))"), 0.0);
    EXPECT_NEAR(evalScalar("imag(sqrt(-4))"), 2.0, 1e-15);
    EXPECT_NEAR(evalScalar("imag(sqrt(-2))"), 1.4142135623730951, 1e-15);
}

TEST_P(ComplexPromotionTest, LogNegativeScalar)
{
    EXPECT_FALSE(evalBool("isreal(log(-1))"));
    EXPECT_DOUBLE_EQ(evalScalar("real(log(-1))"), 0.0);
    EXPECT_NEAR(evalScalar("imag(log(-1))"), M_PI, 1e-13);
    EXPECT_NEAR(evalScalar("real(log(-exp(1)))"), 1.0, 1e-13);
    EXPECT_NEAR(evalScalar("imag(log(-exp(1)))"), M_PI, 1e-13);
}

TEST_P(ComplexPromotionTest, Log2NegativeScalar)
{
    EXPECT_FALSE(evalBool("isreal(log2(-1))"));
    EXPECT_DOUBLE_EQ(evalScalar("real(log2(-1))"), 0.0);
    EXPECT_NEAR(evalScalar("imag(log2(-1))"), 4.5323601418271942, 1e-13);
    EXPECT_NEAR(evalScalar("real(log2(-8))"), 3.0, 1e-13);
    EXPECT_NEAR(evalScalar("imag(log2(-8))"), 4.5323601418271942, 1e-13);
}

TEST_P(ComplexPromotionTest, Log10NegativeScalar)
{
    EXPECT_FALSE(evalBool("isreal(log10(-1))"));
    EXPECT_DOUBLE_EQ(evalScalar("real(log10(-1))"), 0.0);
    EXPECT_NEAR(evalScalar("imag(log10(-1))"), 1.3643763538418412, 1e-13);
    EXPECT_NEAR(evalScalar("real(log10(-100))"), 2.0, 1e-13);
}

// The runtime scalar path (variable, not a literal) must also promote.
TEST_P(ComplexPromotionTest, NegativeVariableScalar)
{
    eval("t = -4;");
    EXPECT_FALSE(evalBool("isreal(sqrt(t))"));
    EXPECT_NEAR(evalScalar("imag(sqrt(t))"), 2.0, 1e-15);
    eval("u = -1;");
    EXPECT_NEAR(evalScalar("imag(log(u))"), M_PI, 1e-13);
}

// Nonnegative inputs stay real (fast path still used, no regression).
TEST_P(ComplexPromotionTest, NonNegativeStaysReal)
{
    EXPECT_TRUE(evalBool("isreal(sqrt(4))"));
    EXPECT_DOUBLE_EQ(evalScalar("sqrt(4)"), 2.0);
    EXPECT_TRUE(evalBool("isreal(sqrt(0))"));
    EXPECT_TRUE(evalBool("isreal(log(1))"));
    EXPECT_TRUE(evalBool("isreal(log2(8))"));
    EXPECT_DOUBLE_EQ(evalScalar("log2(8)"), 3.0);
    EXPECT_TRUE(evalBool("isreal(log10(100))"));
    EXPECT_DOUBLE_EQ(evalScalar("log10(100)"), 2.0);
}

INSTANTIATE_DUAL(ComplexPromotionTest);
