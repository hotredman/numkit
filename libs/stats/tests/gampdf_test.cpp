// libs/stats/tests/gampdf_test.cpp
// gampdf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GampdfTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GampdfTest, ScalarPDF)
{
    EXPECT_NEAR(evalScalar("gampdf(2, 2, 1)"), 0.2706705664732254, 1e-12);
}

TEST_F(GampdfTest, VectorX)
{
    eval("y = gampdf([0 1 2 5], 2, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_NEAR(evalScalar("y(2)"), 0.3678794411714423, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.2706705664732254, 1e-12);
}

TEST_F(GampdfTest, NegativeXReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("gampdf(-1, 2, 1)"), 0.0);
}

TEST_F(GampdfTest, DensityAtZeroByShape)
{
    EXPECT_TRUE(std::isinf(evalScalar("gampdf(0, 0.5, 1)")));   // a<1 → Inf
    EXPECT_DOUBLE_EQ(evalScalar("gampdf(0, 1, 1)"), 1.0);        // a=1 → 1/b
    EXPECT_DOUBLE_EQ(evalScalar("gampdf(0, 2, 1)"), 0.0);        // a>1 → 0
}

TEST_F(GampdfTest, DegenerateOrInvalidParams)
{
    EXPECT_DOUBLE_EQ(evalScalar("gampdf(2, 0, 1)"), 0.0);        // a=0 (degenerate)
    EXPECT_TRUE(std::isnan(evalScalar("gampdf(2, -1, 1)")));      // a<0
    EXPECT_TRUE(std::isnan(evalScalar("gampdf(2, 2, 0)")));       // b=0
}
