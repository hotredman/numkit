// libs/stats/tests/normpdf_test.cpp
// normpdf. Reference values from MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class NormpdfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(NormpdfTest, StandardNormalAtKeyPoints)
{
    EXPECT_NEAR(evalScalar("normpdf(0)"), 0.3989422804014326, 1e-12);  // 1/√(2π)
    EXPECT_NEAR(evalScalar("normpdf(1)"), 0.2419707245191433, 1e-12);  // standard
}

TEST_F(NormpdfTest, VectorInputs)
{
    eval("y = normpdf([-2 -1 0 1 2]);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.0539909665131880, 1e-12);
    EXPECT_NEAR(evalScalar("y(2)"), 0.2419707245191433, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.3989422804014326, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 0.2419707245191433, 1e-12);  // symmetry
    EXPECT_NEAR(evalScalar("y(5)"), 0.0539909665131880, 1e-12);
}

TEST_F(NormpdfTest, ShiftedDistribution)
{
    // N(5, 1) at peak (x=5) → same value as N(0,1) at 0.
    EXPECT_NEAR(evalScalar("normpdf(5, 5, 1)"), 0.3989422804014326, 1e-12);
}

TEST_F(NormpdfTest, NarrowDistributionAtPeak)
{
    // N(2, 0.5) at x=2.5 (=μ+σ) → 1/(σ√(2π))·exp(-1/2) = 0.4839...
    EXPECT_NEAR(evalScalar("normpdf(2.5, 2, 0.5)"), 0.4839414490382867, 1e-12);
}

TEST_F(NormpdfTest, InvalidSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("normpdf(1, 0,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("normpdf(1, 0, -1)")));
}
