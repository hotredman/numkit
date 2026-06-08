// toolboxes/builtin/tests/accumarray_integer_vals_test.cpp
//
// bugs/builtin/accumarray-integer-vals.md — accumarray accepts integer/logical
// vals (MATLAB R2025b). Output class follows the reducer: sum/prod/mean ->
// double, but max/min PRESERVE the integer class. Offline regression guard.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class AccumarrayIntegerValsTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Branch: default reducer (sum) with int vals -> double (was: throw).
TEST_F(AccumarrayIntegerValsTest, DefaultSumIntToDouble)
{
    eval("s = accumarray([1;2;1], int8([10;20;30]));");
    EXPECT_TRUE(eval("isa(s, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 40.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(2)"), 20.0);
}

// Branch: max/min PRESERVE the integer class.
TEST_F(AccumarrayIntegerValsTest, MaxMinPreserveIntClass)
{
    eval("mx = accumarray([1;2;1], int8([100;100;30]), [], @max);");
    EXPECT_TRUE(eval("isa(mx, 'int8')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(mx(1))"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(mx(2))"), 100.0);
    eval("mn = accumarray([1;2;1], int8([10;20;30]), [], @min);");
    EXPECT_TRUE(eval("isa(mn, 'int8')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(mn(1))"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(mn(2))"), 20.0);
    // uint16 preserved too.
    eval("mxu = accumarray([1;2;1], uint16([100;200;30]), [], @max);");
    EXPECT_TRUE(eval("isa(mxu, 'uint16')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(mxu(2))"), 200.0);
}

// Branch: prod / mean with int vals -> double (not the int class).
TEST_F(AccumarrayIntegerValsTest, ProdMeanToDouble)
{
    eval("pd = accumarray([1;2;1], int8([10;20;30]), [], @prod);");
    EXPECT_TRUE(eval("isa(pd, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("pd(1)"), 300.0);   // 10*30
    eval("mu = accumarray([1;2;1], int8([10;20;30]), [], @mean);");
    EXPECT_TRUE(eval("isa(mu, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("mu(1)"), 20.0);    // mean(10,30)
}

// Branch: 2-D subs + sz arg + fillval with int vals.
TEST_F(AccumarrayIntegerValsTest, TwoDSubsSizeFill)
{
    eval("t2 = accumarray([1 1;2 2], int8([5;7]));");
    EXPECT_TRUE(eval("isa(t2, 'double')").toBool());
    EXPECT_EQ(static_cast<int>(evalScalar("numel(t2)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("t2(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("t2(4)"), 7.0);

    eval("sz3 = accumarray([1;2;1], int8([10;20;30]), [3 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(sz3)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("sz3(3)"), 0.0);    // empty cell -> fill 0

    eval("fv = accumarray([1;3], int8([5;7]), [4 1], @sum, -1);");
    EXPECT_DOUBLE_EQ(evalScalar("fv(2)"), -1.0);    // fillval
    EXPECT_DOUBLE_EQ(evalScalar("fv(4)"), -1.0);
}

// Branch: logical vals default-sum -> double (counts).
TEST_F(AccumarrayIntegerValsTest, LogicalDefaultSum)
{
    eval("lg = accumarray([1;2;1], logical([1;0;1]));");
    EXPECT_TRUE(eval("isa(lg, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("lg(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("lg(2)"), 0.0);
}

// Regression: plain double vals unchanged.
TEST_F(AccumarrayIntegerValsTest, DoubleUnchanged)
{
    eval("dd = accumarray([1;2;1], [10;20;30]);");
    EXPECT_TRUE(eval("isa(dd, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("dd(1)"), 40.0);
    EXPECT_DOUBLE_EQ(evalScalar("dd(2)"), 20.0);
}
