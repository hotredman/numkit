// tests/test_arithmetic.cpp — Literals, arithmetic, logical operators
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

// ============================================================
// Literals and arithmetic
// ============================================================

class ArithmeticTest : public DualEngineTest {};

TEST_P(ArithmeticTest, IntegerLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("42;"), 42.0);
}

TEST_P(ArithmeticTest, FloatLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("3.14;"), 3.14);
}

TEST_P(ArithmeticTest, Addition)
{
    EXPECT_DOUBLE_EQ(evalScalar("2 + 3;"), 5.0);
}

TEST_P(ArithmeticTest, Subtraction)
{
    EXPECT_DOUBLE_EQ(evalScalar("10 - 7;"), 3.0);
}

TEST_P(ArithmeticTest, Multiplication)
{
    EXPECT_DOUBLE_EQ(evalScalar("4 * 5;"), 20.0);
}

TEST_P(ArithmeticTest, Division)
{
    EXPECT_DOUBLE_EQ(evalScalar("15 / 3;"), 5.0);
}

TEST_P(ArithmeticTest, Power)
{
    EXPECT_DOUBLE_EQ(evalScalar("2 ^ 10;"), 1024.0);
}

TEST_P(ArithmeticTest, UnaryMinus)
{
    EXPECT_DOUBLE_EQ(evalScalar("-5;"), -5.0);
}

TEST_P(ArithmeticTest, UnaryPlus)
{
    EXPECT_DOUBLE_EQ(evalScalar("+5;"), 5.0);
}

TEST_P(ArithmeticTest, UnaryPlusOnVariable)
{
    eval("x = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("+x;"), 42.0);
}

TEST_P(ArithmeticTest, Precedence)
{
    EXPECT_DOUBLE_EQ(evalScalar("2 + 3 * 4;"), 14.0);
    EXPECT_DOUBLE_EQ(evalScalar("(2 + 3) * 4;"), 20.0);
}

TEST_P(ArithmeticTest, NegPowerPrecedence)
{
    // -2^2 = -(2^2) = -4
    EXPECT_DOUBLE_EQ(evalScalar("-2^2;"), -4.0);
}

TEST_P(ArithmeticTest, PowerRightAssoc)
{
    // 2^3^2 = 2^(3^2) = 2^9 = 512
    EXPECT_DOUBLE_EQ(evalScalar("2^3^2;"), 512.0);
}

TEST_P(ArithmeticTest, HexLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("0xFF;"), 255.0);
}

TEST_P(ArithmeticTest, BinaryLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("0b1010;"), 10.0);
}

TEST_P(ArithmeticTest, OctalLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("0o17;"), 15.0);
}

TEST_P(ArithmeticTest, UnderscoreLiteral)
{
    EXPECT_DOUBLE_EQ(evalScalar("1_000;"), 1000.0);
}

TEST_P(ArithmeticTest, ImaginaryLiteral)
{
    auto v = eval("3i;");
    EXPECT_TRUE(v.isComplex());
    EXPECT_DOUBLE_EQ(v.toComplex().imag(), 3.0);
    EXPECT_DOUBLE_EQ(v.toComplex().real(), 0.0);
}

TEST_P(ArithmeticTest, ComplexArithmetic)
{
    auto v = eval("(2 + 3i) + (1 - 1i);");
    EXPECT_DOUBLE_EQ(v.toComplex().real(), 3.0);
    EXPECT_DOUBLE_EQ(v.toComplex().imag(), 2.0);
}

INSTANTIATE_DUAL(ArithmeticTest);

// ============================================================
// Logical operators
// ============================================================

class LogicalTest : public DualEngineTest {};

TEST_P(LogicalTest, ScalarAnd)
{
    EXPECT_TRUE(evalBool("true & true;"));
    EXPECT_FALSE(evalBool("true & false;"));
}

TEST_P(LogicalTest, ScalarOr)
{
    EXPECT_TRUE(evalBool("true | false;"));
    EXPECT_FALSE(evalBool("false | false;"));
}

TEST_P(LogicalTest, ShortCircuitAnd)
{
    EXPECT_TRUE(evalBool("true && true;"));
    EXPECT_FALSE(evalBool("true && false;"));
}

TEST_P(LogicalTest, ShortCircuitOr)
{
    EXPECT_TRUE(evalBool("false || true;"));
    EXPECT_FALSE(evalBool("false || false;"));
}

TEST_P(LogicalTest, ElementWiseAndArray)
{
    eval("r = [1 0 1] & [1 1 0];");
    auto *r = getVarPtr("r");
    ASSERT_TRUE(r != nullptr);
    EXPECT_TRUE(r->isLogical());
    ASSERT_EQ(r->numel(), 3u);
    const uint8_t *d = r->logicalData();
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 0);
    EXPECT_EQ(d[2], 0);
}

TEST_P(LogicalTest, ElementWiseOrArray)
{
    eval("r = [1 0 1] | [0 1 0];");
    auto *r = getVarPtr("r");
    ASSERT_TRUE(r != nullptr);
    EXPECT_TRUE(r->isLogical());
    ASSERT_EQ(r->numel(), 3u);
    const uint8_t *d = r->logicalData();
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 1);
    EXPECT_EQ(d[2], 1);
}

TEST_P(LogicalTest, ElementWiseAndScalarBroadcast)
{
    eval("r = 1 & [1 0 1];");
    auto *r = getVarPtr("r");
    ASSERT_TRUE(r != nullptr);
    EXPECT_EQ(r->numel(), 3u);
    const uint8_t *d = r->logicalData();
    EXPECT_EQ(d[0], 1);
    EXPECT_EQ(d[1], 0);
    EXPECT_EQ(d[2], 1);
}

TEST_P(LogicalTest, ElementWiseNotArray)
{
    eval("r = ~[1 0 1];");
    auto *r = getVarPtr("r");
    ASSERT_TRUE(r != nullptr);
    EXPECT_TRUE(r->isLogical());
    ASSERT_EQ(r->numel(), 3u);
    const uint8_t *d = r->logicalData();
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(d[1], 1);
    EXPECT_EQ(d[2], 0);
}

TEST_P(LogicalTest, ElementWiseNotScalar)
{
    EXPECT_TRUE(evalBool("~false;"));
    EXPECT_FALSE(evalBool("~true;"));
}

TEST_P(LogicalTest, PrecedenceAllFourLevels)
{
    // || < && < | < &
    EXPECT_TRUE(evalBool("1 || 0 && 0 | 0 & 1;"));
    EXPECT_TRUE(evalBool("0 || 1 && 1 | 0 & 0;"));
}

INSTANTIATE_DUAL(LogicalTest);
