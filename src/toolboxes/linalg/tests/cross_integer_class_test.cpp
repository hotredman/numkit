// toolboxes/linalg/tests/cross_integer_class_test.cpp
//
// bugs/linalg/cross-integer-class.md — cross preserves the integer class of
// integer operands with MATLAB R2025b's per-operation saturating integer
// arithmetic. Offline regression guard with hardcoded expected values.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CrossIntegerClassTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Branch: same int class -> that class (was: throw "Not a double array").
TEST_F(CrossIntegerClassTest, SameIntClass)
{
    eval("c = cross(int8([1 2 3]), int8([4 5 6]));");
    EXPECT_TRUE(eval("isa(c, 'int8')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(c(1))"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(c(2))"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(c(3))"), -3.0);
    eval("c2 = cross(int32([1 0 0]), int32([0 1 0]));");
    EXPECT_TRUE(eval("isa(c2, 'int32')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(c2(3))"), 1.0);
    eval("c16 = cross(int16([10 20 30]), int16([40 50 60]));");
    EXPECT_TRUE(eval("isa(c16, 'int16')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(c16(2))"), 600.0);
}

// Branch: PER-OPERATION saturation — each product saturates before the
// subtraction. cross(int8([100 100 0]),int8([0 100 100])) = [127 -127 127],
// NOT [127 -128 127] (which compute-in-double-then-narrow would give).
TEST_F(CrossIntegerClassTest, PerOperationSaturation)
{
    eval("cs = cross(int8([100 100 0]), int8([0 100 100]));");
    EXPECT_TRUE(eval("isa(cs, 'int8')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(cs(1))"), 127.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(cs(2))"), -127.0);   // NOT -128
    EXPECT_DOUBLE_EQ(evalScalar("double(cs(3))"), 127.0);
}

// Branch: unsigned saturation — negative components clamp to 0.
TEST_F(CrossIntegerClassTest, UnsignedSaturatesToZero)
{
    eval("cu = cross(uint8([1 2 3]), uint8([4 5 6]));");
    EXPECT_TRUE(eval("isa(cu, 'uint8')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(cu(1))"), 0.0);   // -3 -> 0
    EXPECT_DOUBLE_EQ(evalScalar("double(cu(2))"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(cu(3))"), 0.0);   // -3 -> 0
}

// Branch: integer + real double (any shape) -> the integer class.
TEST_F(CrossIntegerClassTest, IntPlusDouble)
{
    EXPECT_TRUE(eval("isa(cross(int8([1 2 3]), [4 5 6]), 'int8')").toBool());
    EXPECT_TRUE(eval("isa(cross([1 2 3], int8([4 5 6])), 'int8')").toBool());
    eval("cd = cross(int8([1 2 3]), [4 5 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(cd(2))"), 6.0);
}

// Branch: Nx3 column layout also preserves class.
TEST_F(CrossIntegerClassTest, ColumnLayout)
{
    eval("cn = cross(int8([1;2;3]), int8([4;5;6]));");
    EXPECT_TRUE(eval("isa(cn, 'int8')").toBool());
    EXPECT_EQ(static_cast<int>(evalScalar("size(cn,1)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("double(cn(1))"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(cn(3))"), -3.0);
}

// Regression: double*double stays double.
TEST_F(CrossIntegerClassTest, DoubleUnchanged)
{
    eval("dd = cross([1 0 0], [0 1 0]);");
    EXPECT_TRUE(eval("isa(dd, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("dd(3)"), 1.0);
}
