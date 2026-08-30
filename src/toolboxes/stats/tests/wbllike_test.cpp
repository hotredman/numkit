// toolboxes/stats/tests/wbllike_test.cpp
// wbllike.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WbllikeTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

static const char *kSetup =
    "data = [1 2 3 4 5]';"
    "cens = [0 0 0 1 1]';"
    "freq = [2 2 1 1 1]';";

TEST_F(WbllikeTest, BasicNoOptions)
{
    eval(kSetup);
    EXPECT_NEAR(evalScalar("wbllike([1, 2], data)"), 46.7467723544, 1e-9);
}

TEST_F(WbllikeTest, WithCensoring)
{
    eval(kSetup);
    EXPECT_NEAR(evalScalar("wbllike([1, 2], data, cens)"), 51.1287989891, 1e-9);
}

TEST_F(WbllikeTest, WithFreq)
{
    eval(kSetup);
    EXPECT_NEAR(evalScalar("wbllike([1, 2], data, [], freq)"), 49.6673308127, 1e-9);
}

TEST_F(WbllikeTest, WithCensoringAndFreq)
{
    eval(kSetup);
    EXPECT_NEAR(evalScalar("wbllike([1, 2], data, cens, freq)"), 54.0493574474, 1e-9);
}

TEST_F(WbllikeTest, InvalidParams)
{
    eval(kSetup);
    // fix #5: invalid params -> NaN (was +Inf).
    EXPECT_TRUE(std::isnan(evalScalar("wbllike([0, 2], data)")));   // scale=0
    EXPECT_TRUE(std::isnan(evalScalar("wbllike([-1, 2], data)")));  // scale<0
    EXPECT_TRUE(std::isnan(evalScalar("wbllike([1, 0], data)")));   // shape=0
    EXPECT_TRUE(std::isnan(evalScalar("wbllike([1, -1], data)")));  // shape<0
}

TEST_F(WbllikeTest, NonPositiveData)
{
    // Weibull support is x > 0; any non-positive x -> NaN (was +Inf).
    EXPECT_TRUE(std::isnan(evalScalar("wbllike([1, 2], [-1; 2; 3])")));
    EXPECT_TRUE(std::isnan(evalScalar("wbllike([1, 2], [0; 2; 3])")));
}
