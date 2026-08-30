// toolboxes/stats/tests/tinv_test.cpp
// tinv.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TinvTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(TinvTest, Quantile975_Nu5)
{
    EXPECT_NEAR(evalScalar("tinv(0.975, 5)"), 2.5705818356363048, 1e-12);
}

TEST_F(TinvTest, VectorQuantile)
{
    eval("v = tinv([0.05 0.5 0.975], 10);");
    EXPECT_NEAR(evalScalar("v(1)"), -1.8124611228116760, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 0.0);
    EXPECT_NEAR(evalScalar("v(3)"), 2.2281388519862735, 1e-12);
}

TEST_F(TinvTest, SmallNu)
{
    EXPECT_NEAR(evalScalar("tinv(0.975, 1)"), 12.7062047361746941, 1e-9);
    EXPECT_NEAR(evalScalar("tinv(0.975, 2)"), 4.3026527297494619, 1e-12);
}

TEST_F(TinvTest, GaussianLimit)
{
    // nu = Inf -> tinv(p, Inf) == norminv(p)
    EXPECT_DOUBLE_EQ(evalScalar("tinv(0.5, Inf)"), 0.0);
    EXPECT_NEAR(evalScalar("tinv(0.975, Inf)"), 1.9599639845400540, 1e-9);
}

TEST_F(TinvTest, BoundaryProb)
{
    EXPECT_TRUE(std::isinf(evalScalar("tinv(0, 5)")));
    EXPECT_LT(evalScalar("tinv(0, 5)"), 0.0);
    EXPECT_TRUE(std::isinf(evalScalar("tinv(1, 5)")));
    EXPECT_GT(evalScalar("tinv(1, 5)"), 0.0);
}

TEST_F(TinvTest, EdgeCases)
{
    EXPECT_TRUE(std::isnan(evalScalar("tinv(-0.1, 5)")));
    EXPECT_TRUE(std::isnan(evalScalar("tinv( 1.5, 5)")));
    EXPECT_TRUE(std::isnan(evalScalar("tinv( 0.5, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("tinv( 0.5, -1)")));
}
