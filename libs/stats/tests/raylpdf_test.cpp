// libs/stats/tests/raylpdf_test.cpp
// Audit ТЗ closure for raylpdf. Closes audit/findings/stats/raylpdf.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RaylpdfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RaylpdfTest, ScalarPDF)
{
    EXPECT_NEAR(evalScalar("raylpdf(2, 1)"), 0.2706705664732254, 1e-12);
}

TEST_F(RaylpdfTest, VectorX)
{
    eval("y = raylpdf([0 1 2 5], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);  // density at 0 is 0
    EXPECT_NEAR(evalScalar("y(2)"), 0.6065306597126334, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 0.2706705664732254, 1e-12);
}

TEST_F(RaylpdfTest, EdgeCases)
{
    EXPECT_DOUBLE_EQ(evalScalar("raylpdf(-1, 1)"), 0.0);
    EXPECT_TRUE(std::isnan(evalScalar("raylpdf(2,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("raylpdf(2, -1)")));
}
