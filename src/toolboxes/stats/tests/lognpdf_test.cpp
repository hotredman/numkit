// toolboxes/stats/tests/lognpdf_test.cpp
// lognpdf.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LognpdfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(LognpdfTest, DefaultMuSigma)
{
    EXPECT_NEAR(evalScalar("lognpdf(2)"), 0.1568740193, 1e-9);
}

TEST_F(LognpdfTest, ScalarPDF)
{
    EXPECT_NEAR(evalScalar("lognpdf(2, 0, 1)"), 0.1568740193, 1e-9);
}

TEST_F(LognpdfTest, VectorX)
{
    eval("y = lognpdf([0 1 2 5], 0, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);  // x=0
    EXPECT_NEAR(evalScalar("y(2)"), 0.3989422804, 1e-9);
    EXPECT_NEAR(evalScalar("y(3)"), 0.1568740193, 1e-9);
}

TEST_F(LognpdfTest, NonPositiveXReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("lognpdf(-1, 0, 1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("lognpdf( 0, 0, 1)"), 0.0);
}

TEST_F(LognpdfTest, InvalidSigmaReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("lognpdf(2, 0,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("lognpdf(2, 0, -1)")));
}
