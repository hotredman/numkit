// libs/stats/tests/poisstat_test.cpp
// poisstat.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class PoisstatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(PoisstatTest, ScalarMomentsAreLambda)
{
    eval("[m, v] = poisstat(2);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("v"), 2.0);  // Poisson: mean = variance = lambda
}

TEST_F(PoisstatTest, VectorInputs)
{
    eval("[m, v] = poisstat([1 2 5 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"), 10.0);
}

TEST_F(PoisstatTest, InvalidLambdaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("poisstat(0)")));   // degenerate
    EXPECT_TRUE(std::isnan(evalScalar("poisstat(-1)")));
}
