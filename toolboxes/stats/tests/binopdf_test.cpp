// toolboxes/stats/tests/binopdf_test.cpp
// binopdf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BinopdfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BinopdfTest, ScalarPMF)
{
    EXPECT_NEAR(evalScalar("binopdf(3, 10, 0.3)"), 0.2668279320, 1e-9);
}

TEST_F(BinopdfTest, VectorK)
{
    eval("y = binopdf([0 3 5 10], 10, 0.3);");
    EXPECT_NEAR(evalScalar("y(1)"), 0.0282475249, 1e-9);
    EXPECT_NEAR(evalScalar("y(2)"), 0.2668279320, 1e-9);
    EXPECT_NEAR(evalScalar("y(3)"), 0.1029193452, 1e-9);
    EXPECT_NEAR(evalScalar("y(4)"), 5.9049e-06,    1e-9);
}

TEST_F(BinopdfTest, OutOfSupportReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("binopdf(-1,  10, 0.3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("binopdf(11,  10, 0.3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("binopdf(3.5, 10, 0.3)"), 0.0);
}

TEST_F(BinopdfTest, BoundaryProbabilities)
{
    EXPECT_DOUBLE_EQ(evalScalar("binopdf( 0, 10, 0)"), 1.0);  // p=0: only k=0
    EXPECT_DOUBLE_EQ(evalScalar("binopdf( 3, 10, 0)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("binopdf(10, 10, 1)"), 1.0);  // p=1: only k=n
    EXPECT_DOUBLE_EQ(evalScalar("binopdf( 3, 10, 1)"), 0.0);
}

TEST_F(BinopdfTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("binopdf(3, -1, 0.3)")));
    EXPECT_TRUE(std::isnan(evalScalar("binopdf(3, 10, -0.1)")));
}
