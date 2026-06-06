// libs/stats/tests/explike_test.cpp
// explike. Reference values from MATLAB R2025b.
// Covers cens + freq + scalar avar + edge fixes (mu<=0 => NaN,
// empty => 0).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ExplikeTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("x = [1 2 3 4 5]';");
        engine.eval("cens = [0 0 0 1 1]';");
        engine.eval("freq = [2 2 1 1 1]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ExplikeTest, BasicNLogLAndAvar)
{
    eval("[nL, av] = explike(2, x);");
    EXPECT_NEAR(evalScalar("nL"), 10.9657359028, 1e-9);
    EXPECT_NEAR(evalScalar("av"),  0.4,           1e-12);
}

TEST_F(ExplikeTest, WithCensoring)
{
    eval("[nL, av] = explike(2, x, cens);");
    EXPECT_NEAR(evalScalar("nL"),  9.5794415417, 1e-9);
    EXPECT_NEAR(evalScalar("av"),  1.0/3.0,       1e-12);
}

TEST_F(ExplikeTest, WithFreq)
{
    eval("[nL, av] = explike(2, x, [], freq);");
    EXPECT_NEAR(evalScalar("nL"), 13.8520302639, 1e-9);
    EXPECT_NEAR(evalScalar("av"),  4.0/11.0,      1e-12);
}

TEST_F(ExplikeTest, CensoringPlusFreq)
{
    eval("[nL, av] = explike(2, x, cens, freq);");
    EXPECT_NEAR(evalScalar("nL"), 12.4657359028, 1e-9);
    EXPECT_NEAR(evalScalar("av"),  4.0/13.0,      1e-12);
}

TEST_F(ExplikeTest, NegativeMuReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("explike(-1, x)")));
}

TEST_F(ExplikeTest, ZeroMuReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("explike(0, x)")));
}

TEST_F(ExplikeTest, EmptyDataReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("explike(2, [])"), 0.0);
}

TEST_F(ExplikeTest, ZeroFreqDropsElement)
{
    eval("y1 = explike(2, x, [], [1 1 1 1 0]');");
    eval("y2 = explike(2, x(1:4));");
    EXPECT_DOUBLE_EQ(evalScalar("y1"), evalScalar("y2"));
}
