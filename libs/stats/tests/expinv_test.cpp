// libs/stats/tests/expinv_test.cpp
// expinv. Reference values from MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ExpinvTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ExpinvTest, DefaultMu1)
{
    // 1-arg form: expinv(p) ≡ expinv(p, 1) → -log(1-p).
    eval("x = expinv([0.05 0.5 0.95]);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.0512932943875505, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 0.6931471805599453, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 2.9957322735539900, 1e-12);
}

TEST_F(ExpinvTest, NonDefaultMu)
{
    eval("x = expinv([0.05 0.5 0.95], 2);");
    EXPECT_NEAR(evalScalar("x(1)"), 0.1025865887751011, 1e-12);
    EXPECT_NEAR(evalScalar("x(2)"), 1.3862943611198906, 1e-12);
    EXPECT_NEAR(evalScalar("x(3)"), 5.9914645471079799, 1e-12);
    EXPECT_NEAR(evalScalar("expinv(0.5, 5)"), 3.4657359027997265, 1e-12);
}

TEST_F(ExpinvTest, BoundaryProbabilities)
{
    EXPECT_DOUBLE_EQ(evalScalar("expinv(0.0)"), 0.0);
    EXPECT_TRUE(std::isinf(evalScalar("expinv(1.0)")));
}

TEST_F(ExpinvTest, OutOfRangeProbReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("expinv(-0.1)")));
    EXPECT_TRUE(std::isnan(evalScalar("expinv( 1.5)")));
}

TEST_F(ExpinvTest, InvalidMuReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("expinv(0.5,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("expinv(0.5, -1)")));
}
