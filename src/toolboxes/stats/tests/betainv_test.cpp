// toolboxes/stats/tests/betainv_test.cpp
// betainv — coverage gap fix (no behavioral
// change). Reference values from MATLAB R2025b probe.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BetainvTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BetainvTest, UniformBeta11Identity)
{
    // Beta(1,1) ≡ Uniform(0,1) → betainv(p, 1, 1) = p.
    EXPECT_NEAR(evalScalar("betainv(0.05, 1, 1)"), 0.05, 1e-12);
    EXPECT_NEAR(evalScalar("betainv(0.50, 1, 1)"), 0.50, 1e-12);
    EXPECT_NEAR(evalScalar("betainv(0.95, 1, 1)"), 0.95, 1e-12);
}

TEST_F(BetainvTest, SymmetricArcsine05)
{
    // Beta(0.5, 0.5) is the arcsine distribution; symmetric about 0.5.
    EXPECT_NEAR(evalScalar("betainv(0.05, 0.5, 0.5)"), 0.0061558288, 1e-9);
    EXPECT_NEAR(evalScalar("betainv(0.50, 0.5, 0.5)"), 0.5,         1e-12);
    EXPECT_NEAR(evalScalar("betainv(0.95, 0.5, 0.5)"), 0.9938441712, 1e-9);
}

TEST_F(BetainvTest, AsymmetricBeta25)
{
    EXPECT_NEAR(evalScalar("betainv(0.05, 2, 5)"), 0.0628498917083544, 1e-12);
    EXPECT_NEAR(evalScalar("betainv(0.50, 2, 5)"), 0.2644499832956600, 1e-12);
    EXPECT_NEAR(evalScalar("betainv(0.95, 2, 5)"), 0.5818034092520256, 1e-12);
}

TEST_F(BetainvTest, NarrowBeta1010)
{
    // Symmetric, peaked at 0.5 — narrow CI.
    EXPECT_NEAR(evalScalar("betainv(0.05, 10, 10)"), 0.3200865295887242, 1e-12);
    EXPECT_NEAR(evalScalar("betainv(0.50, 10, 10)"), 0.5,                1e-12);
    EXPECT_NEAR(evalScalar("betainv(0.95, 10, 10)"), 0.6799134704112758, 1e-12);
}

TEST_F(BetainvTest, BoundaryProbabilities)
{
    EXPECT_DOUBLE_EQ(evalScalar("betainv(0.0, 2, 3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("betainv(1.0, 2, 3)"), 1.0);
}

TEST_F(BetainvTest, OutOfRangeProbReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("betainv(-0.1, 2, 3)")));
    EXPECT_TRUE(std::isnan(evalScalar("betainv( 1.1, 2, 3)")));
}

TEST_F(BetainvTest, InvalidShapeReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("betainv(0.5,  0, 3)")));
    EXPECT_TRUE(std::isnan(evalScalar("betainv(0.5,  2, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("betainv(0.5, -1, 3)")));
}
