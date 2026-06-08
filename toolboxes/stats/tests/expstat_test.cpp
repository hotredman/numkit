// toolboxes/stats/tests/expstat_test.cpp
// expstat. Reference values from MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ExpstatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ExpstatTest, ScalarMomentsAreMuAndMuSquared)
{
    eval("[m, v] = expstat(2);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 2.0);   // mean = mu
    EXPECT_DOUBLE_EQ(evalScalar("v"), 4.0);   // variance = mu²
}

TEST_F(ExpstatTest, VectorInputs)
{
    eval("[m, v] = expstat([1 2 5 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"),   2.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"),   5.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(4)"),  10.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(4)"), 100.0);
}

TEST_F(ExpstatTest, InvalidMuReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("expstat(0)")));
    EXPECT_TRUE(std::isnan(evalScalar("expstat(-1)")));
}
