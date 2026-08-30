// toolboxes/stats/tests/betastat_test.cpp
// betastat. Reference values from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BetastatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BetastatTest, ScalarMeanAndVariance)
{
    eval("[m, v] = betastat(2, 3);");
    EXPECT_NEAR(evalScalar("m"), 0.4,  1e-12);   // a/(a+b) = 2/5
    EXPECT_NEAR(evalScalar("v"), 0.04, 1e-12);   // ab/((a+b)^2(a+b+1)) = 6/(25*6)
}

TEST_F(BetastatTest, UniformBetaIs1_12)
{
    // Beta(1,1) ≡ Uniform(0,1) → m=0.5, v=1/12.
    eval("[m, v] = betastat(1, 1);");
    EXPECT_NEAR(evalScalar("m"), 0.5,         1e-12);
    EXPECT_NEAR(evalScalar("v"), 1.0/12.0,    1e-12);
}

TEST_F(BetastatTest, VectorBroadcasting)
{
    // MATLAB-style: same-size vectors → element-wise.
    eval("[m, v] = betastat([0.5 1 2 5 10], [0.5 1 5 5 10]);");
    EXPECT_NEAR(evalScalar("m(1)"), 0.5,        1e-12);
    EXPECT_NEAR(evalScalar("m(3)"), 2.0/7.0,    1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), 1.0/12.0,   1e-12);
}

TEST_F(BetastatTest, ScalarBroadcastWithVector)
{
    // betastat(2, [1 2 3]) → m = 2/(2+b), v = 2b/(...)
    eval("[m, v] = betastat(2, [1 2 3]);");
    EXPECT_NEAR(evalScalar("m(1)"), 2.0/3.0,  1e-12);
    EXPECT_NEAR(evalScalar("m(2)"), 0.5,      1e-12);
    EXPECT_NEAR(evalScalar("m(3)"), 0.4,      1e-12);
}

TEST_F(BetastatTest, InvalidShapeReturnsNaN)
{
    eval("[m, v] = betastat(0, 3);");
    EXPECT_TRUE(std::isnan(evalScalar("m")));
    EXPECT_TRUE(std::isnan(evalScalar("v")));

    eval("[m, v] = betastat(2, -1);");
    EXPECT_TRUE(std::isnan(evalScalar("m")));
    EXPECT_TRUE(std::isnan(evalScalar("v")));
}
