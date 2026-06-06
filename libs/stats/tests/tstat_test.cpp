// libs/stats/tests/tstat_test.cpp
// tstat.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TstatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(TstatTest, ScalarMomentsForNu5)
{
    // nu=5: mean=0, variance = 5/(5-2) = 5/3.
    eval("[m, v] = tstat(5);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 0.0);
    EXPECT_NEAR(evalScalar("v"), 5.0/3.0, 1e-12);
}

TEST_F(TstatTest, VectorInputs)
{
    eval("[m, v] = tstat([3 5 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"),  3.0);          // 3/(3-2) = 3
    EXPECT_NEAR(evalScalar("v(3)"),       10.0/8.0,  1e-12);
}

TEST_F(TstatTest, Nu2_VarianceUndefined)
{
    eval("[m, v] = tstat(2);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 0.0);              // mean defined for nu>1
    EXPECT_TRUE(std::isnan(evalScalar("v")));            // variance needs nu>2
}

TEST_F(TstatTest, Nu1_MeanAlsoNaN)
{
    // Cauchy distribution (t with nu=1) has no defined mean.
    EXPECT_TRUE(std::isnan(evalScalar("tstat(1)")));
}

TEST_F(TstatTest, InvalidNuReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("tstat(0)")));
    EXPECT_TRUE(std::isnan(evalScalar("tstat(-1)")));
}
