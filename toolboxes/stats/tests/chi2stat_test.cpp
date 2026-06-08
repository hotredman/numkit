// toolboxes/stats/tests/chi2stat_test.cpp
// chi2stat. Reference values from MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Chi2statTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Chi2statTest, ScalarMomentsAreKAndTwoK)
{
    eval("[m, v] = chi2stat(5);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 5.0);   // mean = k
    EXPECT_DOUBLE_EQ(evalScalar("v"), 10.0);  // variance = 2k
}

TEST_F(Chi2statTest, VectorInputs)
{
    eval("[m, v] = chi2stat([1 5 10 30]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(4)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"),  2.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"), 60.0);
}

TEST_F(Chi2statTest, DegenerateKReturnsNaN)
{
    // MATLAB convention: k <= 0 ⇒ moments NaN.
    EXPECT_TRUE(std::isnan(evalScalar("chi2stat(0)")));
    EXPECT_TRUE(std::isnan(evalScalar("chi2stat(-1)")));
}
