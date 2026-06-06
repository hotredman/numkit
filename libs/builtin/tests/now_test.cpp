// libs/builtin/tests/now_test.cpp
//
// Regression guard for now() (MATLAB serial date number).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class NowTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(NowTest, ReturnsScalar)
{
    eval("n = now;");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(n)")), 1);
}

TEST_F(NowTest, InValidRange)
{
    // Serial date for 2025-2030 should be in [739000, 750000].
    EXPECT_GT(evalScalar("now"), 739000.0);
    EXPECT_LT(evalScalar("now"), 750000.0);
}

TEST_F(NowTest, MonotonicIncreasing)
{
    // Two calls in succession should be increasing (or at least
    // non-decreasing within microsecond resolution).
    eval("a = now;");
    // Burn some cycles
    eval("for k = 1:1000; sin(k); end;");
    eval("b = now;");
    EXPECT_GE(evalScalar("b"), evalScalar("a"));
}
