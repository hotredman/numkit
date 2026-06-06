// libs/signal/tests/conv_integer_input_test.cpp
//
// bugs/signal/conv-integer-input.md — conv accepts integer/logical input
// (MATLAB R2025b promotes to double; the result is always double, never the
// integer class). Offline regression guard with hardcoded expected values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ConvIntegerInputTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Branch: int8 * int8 -> double 'full' (was: throw "Not a double array").
TEST_F(ConvIntegerInputTest, Int8FullDouble)
{
    eval("c = conv(int8([1 2 3]), int8([1 1]));");
    EXPECT_TRUE(eval("isa(c, 'double')").toBool());
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(4)"), 3.0);
}

// Branch: shape options 'same' / 'valid' with integer input.
TEST_F(ConvIntegerInputTest, IntShapeOptions)
{
    eval("cs = conv(int8([1 2 3]), int8([1 1]), 'same');");
    EXPECT_TRUE(eval("isa(cs, 'double')").toBool());
    EXPECT_EQ(static_cast<int>(evalScalar("numel(cs)")), 3);
    EXPECT_DOUBLE_EQ(evalScalar("cs(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("cs(3)"), 3.0);

    eval("cv = conv(int8([1 2 3]), int8([1 1]), 'valid');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(cv)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("cv(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("cv(2)"), 5.0);
}

// Branch: mixed int+double and double+int -> double.
TEST_F(ConvIntegerInputTest, MixedIntDouble)
{
    EXPECT_TRUE(eval("isa(conv(int8([1 2 3]), [1 1]), 'double')").toBool());
    EXPECT_TRUE(eval("isa(conv([1 2 3], int8([1 1])), 'double')").toBool());
    eval("m = conv(int8([1 2 3]), [1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 3.0);
}

// Branch: uint and a wider int class (no overflow at double precision).
TEST_F(ConvIntegerInputTest, UintAndInt16)
{
    EXPECT_TRUE(eval("isa(conv(uint8([1 2 3]), uint8([1 1])), 'double')").toBool());
    eval("w = conv(int16([100 200]), int16([2 2]));");
    EXPECT_TRUE(eval("isa(w, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 200.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(2)"), 600.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(3)"), 400.0);
}

// Branch: logical input -> double.
TEST_F(ConvIntegerInputTest, LogicalInput)
{
    eval("L = conv(logical([1 0 1]), [1 1]);");
    EXPECT_TRUE(eval("isa(L, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("L(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(4)"), 1.0);
}

// Regression: plain double*double unchanged.
TEST_F(ConvIntegerInputTest, DoubleUnchanged)
{
    eval("d = conv([1 2 3], [4 5 6]);");
    EXPECT_TRUE(eval("isa(d, 'double')").toBool());
    EXPECT_DOUBLE_EQ(evalScalar("d(2)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(3)"), 28.0);
}
