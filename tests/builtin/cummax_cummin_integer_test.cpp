// toolboxes/builtin/tests/cummax_cummin_integer_test.cpp
//
// Regression guard for bugs/builtin/cummax-cummin-integer.md: cummax/cummin
// used to throw "Not a double array" on integer input (c44 added only a
// logical branch). MATLAB R2025b PRESERVES the integer class (order
// statistics). Values + classes below are bit-exact MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CummaxCumminIntegerTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// cummax(int8) -> int8 [3 3 3 5 5].
TEST_F(CummaxCumminIntegerTest, CummaxInt8)
{
    eval("y = cummax(int8([3 1 2 5 4]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 5.0);
}

// cummin(int8) -> int8 [3 1 1 1 1].
TEST_F(CummaxCumminIntegerTest, CumminInt8)
{
    eval("y = cummin(int8([3 1 2 5 4]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 1.0);
}

// uint16 class preserved.
TEST_F(CummaxCumminIntegerTest, Uint16Class)
{
    eval("y = cummax(uint16([30 10 50 20]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'uint16')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 50.0);
}

// 2-D along dim 2 + 'reverse'.
TEST_F(CummaxCumminIntegerTest, Dim2AndReverse)
{
    eval("c = cummax(int8([3 1; 1 5]), 2);");   // [3 3; 1 5]
    EXPECT_DOUBLE_EQ(evalScalar("c(1,2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2,2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(c,'int8')"), 1.0);
    eval("r = cummax(int8([3 1 2 5 4]), 'reverse');");  // [5 5 5 5 4]
    EXPECT_DOUBLE_EQ(evalScalar("r(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(5)"), 4.0);
}

// Negative signed values.
TEST_F(CummaxCumminIntegerTest, NegativeSigned)
{
    eval("y = cummin(int8([0 -3 2 -5]));");   // [0 -3 -3 -5]
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), -5.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'int8')"), 1.0);
}
