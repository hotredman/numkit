// libs/stats/tests/logninv_test.cpp
// Audit ТЗ closure for logninv. Closes audit/findings/stats/logninv.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LogninvTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(LogninvTest, Median)
{
    EXPECT_DOUBLE_EQ(evalScalar("logninv(0.5, 0, 1)"), 1.0);  // exp(0) = 1
    EXPECT_DOUBLE_EQ(evalScalar("logninv(0.5)"),         1.0);  // default
}

TEST_F(LogninvTest, VectorQ)
{
    eval("x = logninv([0.05 0.5 0.95], 0, 1);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.1930408166987365, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 5.1802516022330103, 1e-12);
}

TEST_F(LogninvTest, BoundaryQuantiles)
{
    EXPECT_DOUBLE_EQ(evalScalar("logninv(0)"), 0.0);
    EXPECT_TRUE(std::isinf(evalScalar("logninv(1)")));
}

TEST_F(LogninvTest, OutOfRangeProbReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("logninv(-0.1)")));
    EXPECT_TRUE(std::isnan(evalScalar("logninv( 1.5)")));
}

TEST_F(LogninvTest, InvalidSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("logninv(0.5, 0,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("logninv(0.5, 0, -1)")));
}
