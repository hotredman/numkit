// toolboxes/stats/tests/evlike_test.cpp
// evlike.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EvlikeTest : public ::testing::Test
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

TEST_F(EvlikeTest, BasicNoOptions)
{
    eval(kSetup);
    EXPECT_NEAR(evalScalar("evlike([0, 1], data)"), 218.2041839863, 1e-9);
}

TEST_F(EvlikeTest, WithCensoring)
{
    eval(kSetup);
    EXPECT_NEAR(evalScalar("evlike([0, 1], data, cens)"), 227.2041839863, 1e-9);
}

TEST_F(EvlikeTest, WithFreq)
{
    eval(kSetup);
    EXPECT_NEAR(evalScalar("evlike([0, 1], data, [], freq)"), 225.3115219137, 1e-9);
}

TEST_F(EvlikeTest, WithCensoringAndFreq)
{
    eval(kSetup);
    EXPECT_NEAR(evalScalar("evlike([0, 1], data, cens, freq)"), 234.3115219137, 1e-9);
}

TEST_F(EvlikeTest, InvalidSigma)
{
    eval(kSetup);
    // fix #5: σ <= 0 -> NaN (was +Inf).
    EXPECT_TRUE(std::isnan(evalScalar("evlike([0, 0], data)")));
    EXPECT_TRUE(std::isnan(evalScalar("evlike([0, -1], data)")));
}

TEST_F(EvlikeTest, EmptyData)
{
    // MATLAB convention: empty -> 0 (was +Inf).
    EXPECT_DOUBLE_EQ(evalScalar("evlike([0, 1], [])"), 0.0);
}
