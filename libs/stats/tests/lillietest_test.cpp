// libs/stats/tests/lillietest_test.cpp
//
// Regression guard for lillietest (Lilliefors normality test).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class LillieTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(LillieTest, NormalLikeNotRejected)
{
    eval("x = [0.1 0.5 -0.3 1.2 -0.7 0.4 -0.1 0.8 -0.4 0.3 0.6 -0.2 0.7 -0.5 0.1];"
         "[h, p, ks, cv] = lillietest(x);");
    EXPECT_DOUBLE_EQ(evalScalar("h"), 0.0);
    EXPECT_GT(evalScalar("p"), 0.05);
}

TEST_F(LillieTest, BimodalRejected)
{
    eval("x = [-3 -3 -3 -3 -3 -3 -3 -3 -3 -3 3 3 3 3 3 3 3 3 3 3];"
         "[h, p, ks, cv] = lillietest(x);");
    EXPECT_DOUBLE_EQ(evalScalar("h"), 1.0);
    EXPECT_LT(evalScalar("p"), 0.05);
}

TEST_F(LillieTest, KSStatComputed)
{
    eval("x = [1 2 3 4 5 6 7 8 9 10]; [h, p, ks, cv] = lillietest(x);");
    EXPECT_GT(evalScalar("ks"), 0.0);
    EXPECT_LT(evalScalar("ks"), 1.0);
    EXPECT_GT(evalScalar("cv"), 0.0);
}

TEST_F(LillieTest, AlphaArgChangesCritval)
{
    eval("x = [1 2 3 4 5 6 7 8 9 10];"
         "[h1, p1, ks1, cv05] = lillietest(x, 0.05);"
         "[h2, p2, ks2, cv01] = lillietest(x, 0.01);");
    // Critical value should be larger for stricter alpha (0.01 vs 0.05).
    EXPECT_GT(evalScalar("cv01"), evalScalar("cv05"));
}

TEST_F(LillieTest, TooFewSamplesThrows)
{
    EXPECT_THROW(eval("lillietest([1 2 3]);"), std::exception);
}

TEST_F(LillieTest, ZeroVarianceThrows)
{
    EXPECT_THROW(eval("lillietest([5 5 5 5 5 5]);"), std::exception);
}
