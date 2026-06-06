// libs/stats/tests/poisspdf_test.cpp
// poisspdf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PoisspdfTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(PoisspdfTest, ScalarPMF)
{
    EXPECT_NEAR(evalScalar("poisspdf(3, 2)"), 0.1804470443, 1e-9);
}

TEST_F(PoisspdfTest, VectorK)
{
    eval("y = poisspdf([0 1 3 10], 2);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.1353352832, 1e-9);
    EXPECT_NEAR(evalScalar("y(3)"), 0.1804470443, 1e-9);
}

TEST_F(PoisspdfTest, OutOfSupportReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("poisspdf(-1,  2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("poisspdf(2.5, 2)"), 0.0);  // non-integer
}

TEST_F(PoisspdfTest, Lambda0Degenerate)
{
    EXPECT_DOUBLE_EQ(evalScalar("poisspdf(0, 0)"), 1.0);  // only k=0
    EXPECT_DOUBLE_EQ(evalScalar("poisspdf(3, 0)"), 0.0);
}

TEST_F(PoisspdfTest, NegativeLambdaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("poisspdf(3, -1)")));
}
