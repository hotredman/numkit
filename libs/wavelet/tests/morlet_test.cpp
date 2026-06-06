// libs/wavelet/tests/morlet_test.cpp
// morlet.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MorletTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Real Morlet: ψ(t) = exp(-t²/2)·cos(5t).
// At t=0: ψ(0) = 1·cos(0) = 1 (peak amplitude).

TEST_F(MorletTest, BasicReferenceValues)
{
    eval("[psi, x] = morlet(-5, 5, 8);");
    EXPECT_NEAR(evalScalar("psi(1)"),  0.0000036938691030, 1e-12);
    EXPECT_NEAR(evalScalar("psi(4)"), -0.7043536746430350, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), -5.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(8)"),  5.0);
}

TEST_F(MorletTest, IsEvenFunction)
{
    // ψ(-t) = ψ(t) since exp(-t²/2)·cos(5t) is even.
    eval("[psi, x] = morlet(-5, 5, 16);");
    EXPECT_NEAR(evalScalar("psi(1)"), evalScalar("psi(16)"), 1e-12);
}

TEST_F(MorletTest, AsymmetricRangeStartsAtPeak)
{
    // At t = 0: ψ(0) = exp(0)·cos(0) = 1.
    eval("[psi, x] = morlet(0, 5, 16);");
    EXPECT_DOUBLE_EQ(evalScalar("psi(1)"), 1.0);
    EXPECT_NEAR(evalScalar("psi(16)"), 0.0000036938691030, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(16)"), 5.0);
}

TEST_F(MorletTest, FineGridReference)
{
    eval("[psi, x] = morlet(-5, 5, 64);");
    EXPECT_NEAR(evalScalar("psi(32)"), 0.9193924929583419, 1e-12);
}
