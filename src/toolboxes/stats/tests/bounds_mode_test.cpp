// toolboxes/stats/tests/bounds_mode_test.cpp
// — 'all' / vecdim dispatch.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace numkit;

class BoundsModeTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
                engine.eval("A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BoundsModeTest, BoundsBasic)
{
    eval("[lo, hi] = bounds(A);");
    EXPECT_DOUBLE_EQ(evalScalar("lo(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("hi(3)"), 11);
}

TEST_F(BoundsModeTest, BoundsAllString)
{
    eval("[lo, hi] = bounds(A, 'all');");
    EXPECT_DOUBLE_EQ(evalScalar("lo"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("hi"), 11);
}

TEST_F(BoundsModeTest, BoundsVecdim)
{
    eval("[lo, hi] = bounds(A, [1 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("lo"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("hi"), 11);
}

TEST_F(BoundsModeTest, ModeBasic)
{
    EXPECT_DOUBLE_EQ(evalScalar("mode([1 2 2 3 3 3])"), 3);
}

TEST_F(BoundsModeTest, ModeAllMatrix)
{
    EXPECT_DOUBLE_EQ(evalScalar("mode(A, 'all')"), 4);
}

// MATLAB R2025b: mode of an empty array -> NaN value, 0 count (not 0x0).
// [M,F]=mode([]) gives M=NaN, F=0; mode(zeros(0,3))=[NaN NaN NaN] with
// F=[0 0 0]; mode(zeros(3,0))=1x0. DEEP-PROBE 2026-05-29.
TEST_F(BoundsModeTest, ModeEmptyIsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("mode([])")));
    EXPECT_DOUBLE_EQ(evalScalar("numel(mode([]))"), 1.0);
    eval("c = mode(zeros(0,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c)"), 3.0);
    EXPECT_TRUE(std::isnan(evalScalar("c(1)")));
    // 2nd output (count F) is 0-shaped, not NaN.
    eval("function [m,f] = md2(v)\n  [m,f] = mode(v);\nend");
    eval("[me, fe] = md2([]);");
    EXPECT_TRUE(std::isnan(evalScalar("me")));
    EXPECT_DOUBLE_EQ(evalScalar("fe"), 0.0);
    eval("[mc, fc] = md2(zeros(0,3));");
    EXPECT_DOUBLE_EQ(evalScalar("fc(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(mode(zeros(3,0)))"), 0.0);
}

TEST_F(BoundsModeTest, ModeVecdim)
{
    EXPECT_DOUBLE_EQ(evalScalar("mode(A, [1 2])"), 4);
}
