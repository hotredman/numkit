// libs/stats/tests/median_extras_test.cpp
//
// Closes audit/closed/stats/median.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MedianExtrasTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("v = [2 5 3 7 4 6 NaN 8 1 9]';");
        engine.eval("A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MedianExtrasTest, DefaultPoisonsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("median(v)")));
}

// MATLAB R2025b: median of an empty array -> NaN (not a 0x0 empty).
// 0x0 [] -> scalar NaN; median(zeros(0,3))=[NaN NaN NaN] (1x3);
// median(zeros(3,0))=1x0; median([],2)=0x1. DEEP-PROBE 2026-05-29.
TEST_F(MedianExtrasTest, EmptyIsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("median([])")));
    EXPECT_DOUBLE_EQ(evalScalar("numel(median([]))"), 1.0);
    eval("c = median(zeros(0,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(c)"), 3.0);
    EXPECT_TRUE(std::isnan(evalScalar("c(2)")));
    EXPECT_DOUBLE_EQ(evalScalar("numel(median(zeros(3,0)))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(median([],2),1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(median([],2),2)"), 1.0);
}

TEST_F(MedianExtrasTest, OmitnanExplicit)
{
    EXPECT_DOUBLE_EQ(evalScalar("median(v, 'omitnan')"), 5);
}

TEST_F(MedianExtrasTest, AllString)
{
    EXPECT_DOUBLE_EQ(evalScalar("median(A, 'all')"), 6);
}

TEST_F(MedianExtrasTest, Vecdim)
{
    EXPECT_DOUBLE_EQ(evalScalar("median(A, [1 2])"), 6);
}

TEST_F(MedianExtrasTest, BasicValuesUnchanged)
{
    EXPECT_DOUBLE_EQ(evalScalar("median([1 2 3 4 5])"), 3);
    eval("y = median(A);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 6);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 9);
}
