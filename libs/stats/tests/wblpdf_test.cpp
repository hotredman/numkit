// libs/stats/tests/wblpdf_test.cpp
// wblpdf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WblpdfTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(WblpdfTest, DefaultsExponentialEquivalent)
{
    // Default a=1, b=1 == exponential PDF e^{-x}.
    EXPECT_NEAR(evalScalar("wblpdf(1)"),   0.3678794411714423, 1e-12);
    EXPECT_NEAR(evalScalar("wblpdf(0.5)"), 0.6065306597126334, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("wblpdf(0)"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("wblpdf(-0.5)"), 0.0);
}

TEST_F(WblpdfTest, WithScaleAndShape)
{
    EXPECT_NEAR(evalScalar("wblpdf(1, 2, 3)"), 0.3309363384692233, 1e-12);
    EXPECT_NEAR(evalScalar("wblpdf(2, 1, 1)"), 0.1353352832366127, 1e-12);
}

TEST_F(WblpdfTest, AtZeroByShape)
{
    // Density at x=0 depends on shape b: b=1 -> 1/a, b<1 -> Inf, b>1 -> 0.
    EXPECT_DOUBLE_EQ(evalScalar("wblpdf(0, 1, 1)"), 1.0);
    EXPECT_TRUE(std::isinf(evalScalar("wblpdf(0, 1, 0.5)")));
    EXPECT_DOUBLE_EQ(evalScalar("wblpdf(0, 1, 2)"), 0.0);
}

TEST_F(WblpdfTest, VectorAcrossSupport)
{
    eval("y = wblpdf([0 0.5 1 2 5], 1, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);          // x=0, b>1 -> 0
    EXPECT_NEAR(evalScalar("y(2)"), 0.7788007830714049, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.7357588823428847, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 0.0732625555549367, 1e-12);
}

TEST_F(WblpdfTest, BadParams)
{
    EXPECT_TRUE(std::isnan(evalScalar("wblpdf(1,  0, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblpdf(1, -1, 1)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblpdf(1,  1, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblpdf(1,  1, -1)")));
    EXPECT_TRUE(std::isnan(evalScalar("wblpdf(1, NaN, 1)")));
}

TEST_F(WblpdfTest, NaNX)
{
    EXPECT_TRUE(std::isnan(evalScalar("wblpdf(NaN, 1, 1)")));
}
