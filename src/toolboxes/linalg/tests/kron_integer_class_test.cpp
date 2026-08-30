// toolboxes/linalg/tests/kron_integer_class_test.cpp
//
// bugs/linalg/kron-integer-class.md — kron preserves the integer class of
// integer operands (saturating, MATLAB R2025b). Offline regression guard
// with hardcoded expected values. One TEST_F per documented branch.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class KronIntegerClassTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Branch: both operands same integer class -> that class, values exact.
TEST_F(KronIntegerClassTest, SameIntClassVector)
{
    eval("k = kron(int8([1 2]), int8([1 1]));");
    EXPECT_TRUE(eval("isa(k, 'int8')").toBool());
    EXPECT_EQ(static_cast<int>(evalScalar("numel(k)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("double(k(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(k(2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(k(3))"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(k(4))"), 2.0);
}

// Branch: same int class, product overflows -> saturating cast.
TEST_F(KronIntegerClassTest, SameIntClassSaturatesHigh)
{
    // int8(100) * int8(2) = 200 -> 127 (max int8).
    EXPECT_TRUE(eval("isa(kron(int8(100), int8(2)), 'int8')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(kron(int8(100), int8(2)))"), 127.0);
    // uint8(200) * uint8(2) = 400 -> 255 (max uint8).
    EXPECT_TRUE(eval("isa(kron(uint8(200), uint8(2)), 'uint8')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(kron(uint8(200), uint8(2)))"), 255.0);
}

TEST_F(KronIntegerClassTest, SameIntClassSaturatesLow)
{
    // int8(-100) * int8(2) = -200 -> -128 (min int8).
    EXPECT_TRUE(eval("isa(kron(int8(-100), int8(2)), 'int8')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("double(kron(int8(-100), int8(2)))"), -128.0);
}

// Branch: integer + real scalar double -> integer class (scalar cast).
TEST_F(KronIntegerClassTest, IntPlusScalarDouble)
{
    EXPECT_TRUE(eval("isa(kron(int8([2 3]), 2), 'int8')").toBool());
    eval("k = kron(int8([2 3]), 2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(k(1))"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(k(2))"), 6.0);
    // Symmetric: scalar double first.
    EXPECT_TRUE(eval("isa(kron(2, int8([2 3])), 'int8')").toBool());
    // Fractional scalar -> round-half-away after multiply.
    EXPECT_DOUBLE_EQ(evalScalar("double(kron(int8(2), 1.5))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(kron(int8(3), 0.5))"), 2.0);  // 1.5 -> 2
}

// Branch: uint16 preserved + correct shape on a non-trivial product.
TEST_F(KronIntegerClassTest, Uint16Shape)
{
    eval("k = kron(uint16([1 2]), uint16([1 0 1]));");
    EXPECT_TRUE(eval("isa(k, 'uint16')").toBool());
    EXPECT_EQ(static_cast<int>(evalScalar("size(k,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(k,2)")), 6);
    EXPECT_DOUBLE_EQ(evalScalar("double(k(4))"), 2.0);  // 2*1
    EXPECT_DOUBLE_EQ(evalScalar("double(k(5))"), 0.0);  // 2*0
}

// Regression: double*double stays double (unchanged path).
TEST_F(KronIntegerClassTest, DoubleStaysDouble)
{
    eval("k = kron([1 2], [3 4]);");
    EXPECT_TRUE(eval("isa(k, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("k(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("k(4)"), 8.0);
}
