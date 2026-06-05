// libs/stats/tests/unifpdf_test.cpp
// unifpdf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UnifpdfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(UnifpdfTest, DefaultsUnitInterval)
{
    // Default a=0, b=1 (1-arg form).
    EXPECT_DOUBLE_EQ(evalScalar("unifpdf(0.5)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("unifpdf(0)"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("unifpdf(1)"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("unifpdf(-0.1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("unifpdf( 1.1)"), 0.0);
}

TEST_F(UnifpdfTest, WiderInterval)
{
    EXPECT_DOUBLE_EQ(evalScalar("unifpdf(2, 1, 5)"), 0.25);
    EXPECT_DOUBLE_EQ(evalScalar("unifpdf(0, 1, 5)"), 0.0);  // below
    EXPECT_DOUBLE_EQ(evalScalar("unifpdf(6, 1, 5)"), 0.0);  // above
}

TEST_F(UnifpdfTest, VectorAcrossSupport)
{
    eval("y = unifpdf([-0.5 0 0.3 1 1.5], 0, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 0.0);
}

TEST_F(UnifpdfTest, BadParams)
{
    EXPECT_TRUE(std::isnan(evalScalar("unifpdf(0.5, 1, 0)")));   // b < a
    EXPECT_TRUE(std::isnan(evalScalar("unifpdf(0.5, 5, 1)")));   // b < a
    EXPECT_TRUE(std::isnan(evalScalar("unifpdf(0.5, 1, 1)")));   // b == a degenerate
}

TEST_F(UnifpdfTest, NaNX)
{
    // MATLAB convention: NaN x -> NaN; NaN params fall through to false
    // comparison and emit 0 (not tested here — MATLAB-only quirk).
    EXPECT_TRUE(std::isnan(evalScalar("unifpdf(NaN, 0, 1)")));
}
