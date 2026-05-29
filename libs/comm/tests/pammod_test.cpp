// libs/comm/tests/pammod_test.cpp
//
// Regression guard for pammod() / pamdemod() — M-PAM modulation.
// MATLAB R2025b: pammod/pamdemod DEFAULT to 'bin' (binary) symbol
// ordering, NOT 'gray' (unlike qammod, which defaults to 'gray').
// numkit previously defaulted all four to 'gray'.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PammodTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(PammodTest, DefaultIsBinaryNotGray)
{
    // Default order = 'bin': symbol k -> 2k-(M-1).
    eval("yb = real(pammod([0 1 2 3], 4));");
    EXPECT_DOUBLE_EQ(evalScalar("yb(1)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("yb(2)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yb(3)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("yb(4)"),  3.0);
}

TEST_F(PammodTest, ExplicitGrayReorders)
{
    // 'gray' maps via gray code -> symbols 2,3 swap to points 3,1.
    eval("yg = real(pammod([0 1 2 3], 4, 0, 'gray'));");
    EXPECT_DOUBLE_EQ(evalScalar("yg(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("yg(4)"), 1.0);
}

TEST_F(PammodTest, M8Binary)
{
    eval("y8 = real(pammod(0:7, 8));");
    EXPECT_DOUBLE_EQ(evalScalar("y8(1)"), -7.0);
    EXPECT_DOUBLE_EQ(evalScalar("y8(2)"), -5.0);
    EXPECT_DOUBLE_EQ(evalScalar("y8(8)"),  7.0);
}

TEST_F(PammodTest, RoundTripBinary)
{
    eval("x = pamdemod(pammod([0 1 2 3], 4), 4);");
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(4)"), 3.0);
}
