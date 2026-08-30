// toolboxes/wavelet/tests/meyeraux_test.cpp
// meyeraux.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MeyerauxTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Element-wise polynomial 35x⁴ − 84x⁵ + 70x⁶ − 20x⁷, clipped to
// 0 for x<=0 and 1 for x>=1.

TEST_F(MeyerauxTest, EndpointsAndCenter)
{
    EXPECT_DOUBLE_EQ(evalScalar("meyeraux(0)"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("meyeraux(0.5)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("meyeraux(1)"),   1.0);
}

TEST_F(MeyerauxTest, VectorOnSupport)
{
    eval("y = meyeraux([0 0.25 0.5 0.75 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_NEAR(evalScalar("y(2)"), 0.0705566406, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 0.5);
    EXPECT_NEAR(evalScalar("y(4)"), 0.9294433594, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 1.0);
}

TEST_F(MeyerauxTest, ClipsBelowZero)
{
    // Bug fix 2026-05-08: numkit used to apply the raw polynomial,
    // returning 6.0625 for x=-0.5. MATLAB R2025b clips to 0.
    EXPECT_DOUBLE_EQ(evalScalar("meyeraux(-0.5)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("meyeraux(-1.0)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("meyeraux(-100)"), 0.0);
}

TEST_F(MeyerauxTest, ClipsAboveOne)
{
    // Numkit used to return -208 for x=2 (raw polynomial). MATLAB clips to 1.
    EXPECT_DOUBLE_EQ(evalScalar("meyeraux(2)"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("meyeraux(1.5)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("meyeraux(100)"), 1.0);
}

TEST_F(MeyerauxTest, MatrixInput)
{
    eval("y = meyeraux([0 0.5; 0.25 0.75]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2)"), 0.5);
    EXPECT_NEAR(evalScalar("y(2,1)"), 0.0705566406, 1e-9);
    EXPECT_NEAR(evalScalar("y(2,2)"), 0.9294433594, 1e-9);
}

TEST_F(MeyerauxTest, ClippingInVector)
{
    eval("y = meyeraux([-1 0 0.5 1 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 1.0);
}
