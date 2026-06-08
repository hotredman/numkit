// toolboxes/stats/tests/unidpdf_test.cpp
// unidpdf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UnidpdfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(UnidpdfTest, InSupport)
{
    EXPECT_DOUBLE_EQ(evalScalar("unidpdf(3, 6)"), 1.0 / 6.0);
}

TEST_F(UnidpdfTest, VectorAcrossSupport)
{
    eval("y = unidpdf([1 2 3 4 5 6 7], 6);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0 / 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1.0 / 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(6)"), 1.0 / 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(7)"), 0.0);  // out of {1..N}
}

TEST_F(UnidpdfTest, OutOfSupport)
{
    EXPECT_DOUBLE_EQ(evalScalar("unidpdf(0, 6)"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("unidpdf(7, 6)"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("unidpdf(2.5, 6)"), 0.0);  // non-integer k
    // MATLAB convention: non-finite k -> 0 (not NaN).
    EXPECT_DOUBLE_EQ(evalScalar("unidpdf(NaN, 6)"), 0.0);
}

TEST_F(UnidpdfTest, BadN)
{
    EXPECT_TRUE(std::isnan(evalScalar("unidpdf(3, 0)")));
    EXPECT_TRUE(std::isnan(evalScalar("unidpdf(3, -1)")));
    EXPECT_TRUE(std::isnan(evalScalar("unidpdf(3, 6.5)")));  // non-integer N
    EXPECT_TRUE(std::isnan(evalScalar("unidpdf(3, NaN)")));
}
