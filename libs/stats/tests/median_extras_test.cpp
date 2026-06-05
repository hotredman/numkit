// libs/stats/tests/median_extras_test.cpp

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

// MATLAB preserves the integer class for median: the two-middle-element
// average is rounded half-away-from-zero and the integer class is kept
// (median(int32([1 2 3 4]))=3 int32, median(int8([-1 -2]))=-2 int8).
// numkit previously returned a DOUBLE 2.5. DEEP-PROBE 2026-05-30.
TEST_F(MedianExtrasTest, IntegerClassPreserved)
{
    eval("mi = median(int32([1 2 3 4]));");        // 2.5 -> 3
    EXPECT_DOUBLE_EQ(evalScalar("double(mi)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(mi),'int32'))"), 1.0);
    eval("mo = median(int32([1 2 3]));");          // odd -> exact 2
    EXPECT_DOUBLE_EQ(evalScalar("double(mo)"), 2.0);
    eval("mn = median(int8([-1 -2]));");           // -1.5 -> -2 (half away)
    EXPECT_DOUBLE_EQ(evalScalar("double(mn)"), -2.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(mn),'int8'))"), 1.0);
    eval("mn2 = median(int8([-2 -3]));");          // -2.5 -> -3
    EXPECT_DOUBLE_EQ(evalScalar("double(mn2)"), -3.0);
    eval("mu = median(uint8([10 20 30 41]));");    // 25 exact
    EXPECT_DOUBLE_EQ(evalScalar("double(mu)"), 25.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(mu),'uint8'))"), 1.0);
    // Per-column for matrices keeps the class.
    eval("mc = median(int32([1 2; 3 4; 5 6]));");
    EXPECT_DOUBLE_EQ(evalScalar("double(mc(1))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(mc(2))"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(mc),'int32'))"), 1.0);
    // double / single inputs are unchanged.
    EXPECT_DOUBLE_EQ(evalScalar("median([1 2 3 4])"), 2.5);
    EXPECT_DOUBLE_EQ(
        evalScalar("double(strcmp(class(median(single([1 2 3 4]))),'single'))"),
        1.0);
}
