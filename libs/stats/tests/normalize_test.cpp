// libs/stats/tests/normalize_test.cpp
//
// Regression guard for normalize's method PARAMETER (range bounds, norm-p,
// scale divisor, center reference) — previously parsed-and-ignored, so
// every method used its default. vs MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class NormalizeParamTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(NormalizeParamTest, RangeBounds)
{
    eval("y = normalize([1 2 3 4 5], 'range', [0 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 10.0);
    eval("z = normalize([1 2 3 4 5], 'range', [-1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("z(1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(5)"), 1.0);
    // default [0 1] still works.
    eval("d = normalize([1 2 3 4 5], 'range');");
    EXPECT_DOUBLE_EQ(evalScalar("d(2)"), 0.25);
}

TEST_F(NormalizeParamTest, NormP)
{
    eval("a = normalize([1 2 3 4 5], 'norm', 1);");   // /sum|x| = /15
    EXPECT_NEAR(evalScalar("a(1)"), 1.0 / 15.0, 1e-12);
    EXPECT_NEAR(evalScalar("a(5)"), 5.0 / 15.0, 1e-12);
    eval("b = normalize([1 2 3 4 5], 'norm', Inf);"); // /max|x| = /5
    EXPECT_DOUBLE_EQ(evalScalar("b(1)"), 0.2);
    EXPECT_DOUBLE_EQ(evalScalar("b(5)"), 1.0);
    eval("c = normalize([1 2 3 4 5], 'norm', 2);");   // default 2-norm = /sqrt(55)
    EXPECT_NEAR(evalScalar("c(1)"), 1.0 / std::sqrt(55.0), 1e-12);
}

TEST_F(NormalizeParamTest, ScaleAndCenterReference)
{
    eval("s = normalize([1 2 3 4 5], 'scale', 'first');");  // /x(1) = /1
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(5)"), 5.0);
    // default 'scale' = std (sample N-1): /sqrt(2.5).
    eval("sd = normalize([1 2 3 4 5], 'scale');");
    EXPECT_NEAR(evalScalar("sd(1)"), 1.0 / std::sqrt(2.5), 1e-12);
    // center 'median' subtracts the median (3 here).
    eval("cm = normalize([1 2 3 4 5], 'center', 'median');");
    EXPECT_DOUBLE_EQ(evalScalar("cm(1)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("cm(5)"), 2.0);
}
