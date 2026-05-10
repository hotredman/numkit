// libs/stats/tests/corrcov_test.cpp
//
// Regression guard for corrcov() — correlation matrix from covariance.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CorrcovTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CorrcovTest, KnownCovariance)
{
    // C = [4 2 1; 2 9 3; 1 3 16] -> sigma = [2 3 4]
    // R(1,2) = 2 / (2*3) = 1/3
    // R(1,3) = 1 / (2*4) = 1/8
    // R(2,3) = 3 / (3*4) = 1/4
    eval("[R, s] = corrcov([4 2 1; 2 9 3; 1 3 16]);");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,3)"), 1.0);
    EXPECT_NEAR(evalScalar("R(1,2)"), 1.0/3.0, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,3)"), 0.125);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,3)"), 0.25);
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(3)"), 4.0);
}

TEST_F(CorrcovTest, IdentityCovariance)
{
    eval("[R, s] = corrcov(eye(3));");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 1.0);
}

TEST_F(CorrcovTest, ScalarInput)
{
    // corrcov(5) -> R = 1, sigma = sqrt(5)
    eval("[R, s] = corrcov(5);");
    EXPECT_DOUBLE_EQ(evalScalar("R"), 1.0);
    EXPECT_NEAR(evalScalar("s"), std::sqrt(5.0), 1e-12);
}

TEST_F(CorrcovTest, NegativeCorrelation)
{
    // C = [4 -2; -2 1] -> R = [1 -1; -1 1]
    eval("R = corrcov([4 -2; -2 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(1,2)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"),  1.0);
}

TEST_F(CorrcovTest, SigmaShape)
{
    eval("[~, s] = corrcov([1 0; 0 4]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(s, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(s, 2)")), 2);
}

TEST_F(CorrcovTest, RejectsNonSquare)
{
    bool threw = false;
    try { eval("corrcov([1 2; 3 4; 5 6]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(CorrcovTest, RejectsNegativeVariance)
{
    bool threw = false;
    try { eval("corrcov([-1 0; 0 1]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(CorrcovTest, ZeroVarianceGivesNaN)
{
    // C(1,1) = 0 -> sigma(1) = 0 -> R(1,*) and R(*,1) involve division by 0.
    eval("R = corrcov([0 0; 0 1]);");
    EXPECT_TRUE(std::isnan(evalScalar("R(1,1)")));
    EXPECT_TRUE(std::isnan(evalScalar("R(1,2)")));
    EXPECT_TRUE(std::isnan(evalScalar("R(2,1)")));
    EXPECT_DOUBLE_EQ(evalScalar("R(2,2)"), 1.0);
}
