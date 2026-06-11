// toolboxes/stats/tests/movfun_order_stats_test.cpp
//
// Regression guard for bugs/stats/movfun-order-stats.md: movmax/movmin/
// movmedian used to throw "Not a double array" on integer/logical input.
// MATLAB R2025b PRESERVES the class for these order statistics:
//   movmax/movmin: int->int (exact), logical->logical
//   movmedian:     int->int (round half away from zero), logical->DOUBLE
// Window 3 unless noted, endpoints 'shrink'. Bit-exact MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MovfunOrderStatsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// movmax(int8) -> int8 [3 3 5 5 5].
TEST_F(MovfunOrderStatsTest, MovmaxInt8)
{
    eval("y = movmax(int8([3 1 2 5 4]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 5.0);
}

// movmin(int8) -> int8 [1 1 1 2 4].
TEST_F(MovfunOrderStatsTest, MovminInt8)
{
    eval("y = movmin(int8([3 1 2 5 4]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 4.0);
}

// movmedian(int8, 3) -> int8 [2 2 2 4 5] (4.5 -> 5 round half away).
TEST_F(MovfunOrderStatsTest, MovmedianInt8RoundsW3)
{
    eval("y = movmedian(int8([3 1 2 5 4]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 5.0);
}

// movmedian(int8, 2) -> [3 2 2 4 5]: fractional medians round half away
// (1.5->2, 3.5->4, 4.5->5). Negative: [-1 -2 -3 -5] (-1.5->-2, -4.5->-5).
TEST_F(MovfunOrderStatsTest, MovmedianRoundHalfAway)
{
    eval("y = movmedian(int8([3 1 2 5 4]), 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 2.0);   // median(1,2)=1.5 -> 2
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 4.0);   // median(2,5)=3.5 -> 4
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 5.0);   // median(5,4)=4.5 -> 5
    eval("n = movmedian(int8([-1 -2 -4 -5]), 2);");
    EXPECT_DOUBLE_EQ(evalScalar("n(2)"), -2.0);  // median(-1,-2)=-1.5 -> -2
    EXPECT_DOUBLE_EQ(evalScalar("n(4)"), -5.0);  // median(-4,-5)=-4.5 -> -5
}

// movmax/movmin on logical -> logical (class preserved).
TEST_F(MovfunOrderStatsTest, MovmaxMovminLogical)
{
    eval("a = movmax(logical([1 0 1 1 0]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(a)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(5)"), 1.0);
    eval("b = movmin(logical([1 0 1 1 0]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(b)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("b(3)"), 0.0);
}

// movmedian on logical -> DOUBLE (a 0.5 median can't be logical).
TEST_F(MovfunOrderStatsTest, MovmedianLogicalToDouble)
{
    eval("y = movmedian(logical([1 0 1 1 0]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(y)"), 0.0);
    EXPECT_NEAR(evalScalar("y(1)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(5)"), 0.5, 1e-12);
}
