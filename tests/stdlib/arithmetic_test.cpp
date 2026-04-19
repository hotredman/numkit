// tests/test_arithmetic.cpp — Literals, arithmetic, logical operators
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

#include <cmath>

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

// ── Implicit expansion (broadcasting) ───────────────────────

TEST_P(ArithmeticTest, BroadcastRowTimesCol)
{
    // [1 2 3] .* [4;5;6] → 3x3 matrix
    eval("A = [1 2 3] .* [4;5;6];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 3u);
    // Row 0: 1*4, 2*4, 3*4
    EXPECT_DOUBLE_EQ((*A)(0, 0), 4.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 8.0);
    EXPECT_DOUBLE_EQ((*A)(0, 2), 12.0);
    // Row 1: 1*5, 2*5, 3*5
    EXPECT_DOUBLE_EQ((*A)(1, 0), 5.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 10.0);
    EXPECT_DOUBLE_EQ((*A)(1, 2), 15.0);
    // Row 2: 1*6, 2*6, 3*6
    EXPECT_DOUBLE_EQ((*A)(2, 0), 6.0);
    EXPECT_DOUBLE_EQ((*A)(2, 1), 12.0);
    EXPECT_DOUBLE_EQ((*A)(2, 2), 18.0);
}

TEST_P(ArithmeticTest, BroadcastColPlusRow)
{
    // [1;2;3] + [10 20 30] → 3x3 matrix
    eval("A = [1;2;3] + [10 20 30];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 3u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 11.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 22.0);
    EXPECT_DOUBLE_EQ((*A)(2, 2), 33.0);
}

TEST_P(ArithmeticTest, BroadcastColMinusRow)
{
    eval("A = [10;20;30] - [1 2 3];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 3u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 9.0);
    EXPECT_DOUBLE_EQ((*A)(2, 2), 27.0);
}

TEST_P(ArithmeticTest, BroadcastMatrixPlusCol)
{
    // [1 2; 3 4] + [10; 20] → add column to each column
    eval("A = [1 2; 3 4] + [10; 20];");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 11.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 23.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 12.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 24.0);
}

TEST_P(ArithmeticTest, BroadcastMatrixTimesRow)
{
    // [1 2; 3 4] .* [10 100] → multiply each row
    eval("A = [1 2; 3 4] .* [10 100];");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0, 0), 10.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 200.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 30.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 400.0);
}

TEST_P(ArithmeticTest, BroadcastElementwisePower)
{
    // [2;3] .^ [1 2 3] → 2x3 matrix
    eval("A = [2;3] .^ [1 2 3];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 2u);
    EXPECT_EQ(cols(*A), 3u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 2.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 4.0);
    EXPECT_DOUBLE_EQ((*A)(0, 2), 8.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 3.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1), 9.0);
    EXPECT_DOUBLE_EQ((*A)(1, 2), 27.0);
}

TEST_P(ArithmeticTest, BroadcastComparison)
{
    // [1;2;3] > [1 2 3] → 3x3 logical
    eval("A = [1;2;3] > [1 2 3];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 3u);
    EXPECT_EQ(cols(*A), 3u);
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

// ============================================================
// 3D heap-safety regressions
//
// These exercise code paths that used to allocate a 2D result buffer
// (MValue::matrix(rows, cols)) and then iterate over numel() elements.
// For 3D inputs numel = rows*cols*pages, so the write ran past the
// end of the heap allocation. All sites now go through createLike()
// which dispatches to matrix3d when pages > 0.
// ============================================================

class HeapSafety3DTest : public DualEngineTest {};

TEST_P(HeapSafety3DTest, UnaryDoubleSin)
{
    eval("A = ones(2, 3, 2); B = sin(A);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(B->type(), MType::DOUBLE);
    EXPECT_TRUE(B->dims().is3D());
    EXPECT_EQ(B->numel(), 12u);
    EXPECT_NEAR(B->doubleData()[0], std::sin(1.0), 1e-12);
    EXPECT_NEAR(B->doubleData()[11], std::sin(1.0), 1e-12);
}

TEST_P(HeapSafety3DTest, UnaryTypedIntegerNegate)
{
    eval("A = int32(ones(2, 3, 2)); B = -A;");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(B->type(), MType::INT32);
    EXPECT_TRUE(B->dims().is3D());
    EXPECT_EQ(B->numel(), 12u);
    EXPECT_EQ(B->int32Data()[0], -1);
    EXPECT_EQ(B->int32Data()[11], -1);
}

TEST_P(HeapSafety3DTest, UnaryComplexConj)
{
    eval("A = ones(2, 2, 2) + 1i; B = conj(A);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_TRUE(B->isComplex());
    EXPECT_TRUE(B->dims().is3D());
    EXPECT_EQ(B->numel(), 8u);
    EXPECT_DOUBLE_EQ(B->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(B->complexData()[0].imag(), -1.0);
    EXPECT_DOUBLE_EQ(B->complexData()[7].imag(), -1.0);
}

TEST_P(HeapSafety3DTest, EqualityDoubleVsScalar)
{
    eval("A = ones(2, 3, 2) * 3; M = (A == 3);");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(M->type(), MType::LOGICAL);
    EXPECT_TRUE(M->dims().is3D());
    EXPECT_EQ(M->numel(), 12u);
    for (size_t i = 0; i < 12; ++i)
        EXPECT_EQ(M->logicalData()[i], 1u);
}

TEST_P(HeapSafety3DTest, EqualityComplexVsScalar)
{
    eval("A = ones(2, 2, 2) + 0i; M = (A == 1);");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(M->type(), MType::LOGICAL);
    EXPECT_TRUE(M->dims().is3D());
    EXPECT_EQ(M->numel(), 8u);
    for (size_t i = 0; i < 8; ++i)
        EXPECT_EQ(M->logicalData()[i], 1u);
}

TEST_P(HeapSafety3DTest, LogicalAndBothArrays)
{
    eval("A = ones(2, 2, 2); B = zeros(2, 2, 2); B(1) = 1; M = A & B;");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(M->type(), MType::LOGICAL);
    EXPECT_TRUE(M->dims().is3D());
    EXPECT_EQ(M->numel(), 8u);
    EXPECT_EQ(M->logicalData()[0], 1u);
    for (size_t i = 1; i < 8; ++i)
        EXPECT_EQ(M->logicalData()[i], 0u);
}

TEST_P(HeapSafety3DTest, LogicalOrBothArrays)
{
    eval("A = zeros(2, 2, 2); B = zeros(2, 2, 2); B(3) = 1; M = A | B;");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(M->type(), MType::LOGICAL);
    EXPECT_TRUE(M->dims().is3D());
    EXPECT_EQ(M->numel(), 8u);
    EXPECT_EQ(M->logicalData()[2], 1u);
}

INSTANTIATE_DUAL(HeapSafety3DTest);
