// toolboxes/signal/tests/firpmord_test.cpp
//
// Regression guard for firpmord (Phase 4.7). Bit-equal MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FirpmordTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(FirpmordTest, LowpassHelpExample)
{
    eval("[n, fo, ao, w] = firpmord([1500 2000], [1 0], [0.01 0.1], 8000);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 21);
    EXPECT_NEAR(evalScalar("fo(2)"), 0.375, 1e-9);
    EXPECT_NEAR(evalScalar("fo(3)"), 0.5,   1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("ao(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ao(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ao(3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ao(4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(2)"), 1.0);
}

TEST_F(FirpmordTest, Highpass)
{
    eval("n = firpmord([800 1000], [0 1], [0.01 0.05], 4000);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 32);
}

TEST_F(FirpmordTest, BandpassMultiband)
{
    eval("n = firpmord([500 1000 2000 2500], [0 1 0], [0.05 0.01 0.05], 8000);");
    EXPECT_EQ(static_cast<int>(evalScalar("n")), 24);
}

TEST_F(FirpmordTest, OddOrderBumpAtNyquist)
{
    // If gain at Nyquist != 0 and order is odd, firpmord increments.
    // Highpass [0 1] case has Nyquist gain = 1, so odd N → bumped.
    eval("n_hp = firpmord([800 1000], [0 1], [0.01 0.05], 4000);");
    EXPECT_EQ(static_cast<int>(evalScalar("mod(n_hp, 2)")), 0);  // even after bump
}
