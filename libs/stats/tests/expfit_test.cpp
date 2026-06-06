// libs/stats/tests/expfit_test.cpp
// expfit.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ExpfitTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Reference inputs (match parity spec).
static const char *kSetup =
    "x    = [1 2 3 4 5 6 7 8 9 10]';"
    "cens = [0 0 0 0 0 0 0 1 1 1]';"
    "freq = [2 2 2 1 1 1 1 1 1 1]';";

TEST_F(ExpfitTest, BasicNoOptions)
{
    eval(kSetup);
    eval("[mu, ci] = expfit(x);");
    EXPECT_NEAR(evalScalar("mu"),    5.5,                 1e-12);
    EXPECT_NEAR(evalScalar("ci(1)"), 3.2192351616031876,  1e-9);
    EXPECT_NEAR(evalScalar("ci(2)"), 11.4693518055018906, 1e-9);
}

TEST_F(ExpfitTest, WithCensoring)
{
    eval(kSetup);
    eval("[mu, ci] = expfit(x, 0.05, cens);");
    EXPECT_NEAR(evalScalar("mu"),    7.857142857142857,   1e-12);
    EXPECT_NEAR(evalScalar("ci(1)"), 4.2115019261,        1e-9);
    EXPECT_NEAR(evalScalar("ci(2)"), 19.5426101726,       1e-9);
}

TEST_F(ExpfitTest, WithFreq)
{
    eval(kSetup);
    eval("[mu, ci] = expfit(x, 0.05, [], freq);");
    EXPECT_NEAR(evalScalar("mu"),    4.692307692307692,   1e-12);
    EXPECT_NEAR(evalScalar("ci(1)"), 2.9100852755,        1e-9);
    EXPECT_NEAR(evalScalar("ci(2)"), 8.8125424263,        1e-9);
}

TEST_F(ExpfitTest, WithCensoringAndFreq)
{
    eval(kSetup);
    eval("[mu, ci] = expfit(x, 0.05, cens, freq);");
    EXPECT_NEAR(evalScalar("mu"),    6.1,                 1e-12);
    EXPECT_NEAR(evalScalar("ci(1)"), 3.5704244520,        1e-9);
    EXPECT_NEAR(evalScalar("ci(2)"), 12.7205538206,       1e-9);
}

TEST_F(ExpfitTest, EdgeAllCensored)
{
    // If every observation is censored, no events -> NaN.
    eval("xs = [1 2 3]'; allcens = [1 1 1]';");
    eval("[mu, ci] = expfit(xs, 0.05, allcens);");
    EXPECT_TRUE(std::isnan(evalScalar("mu")));
    EXPECT_TRUE(std::isnan(evalScalar("ci(1)")));
    EXPECT_TRUE(std::isnan(evalScalar("ci(2)")));
}
