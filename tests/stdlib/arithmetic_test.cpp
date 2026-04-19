// tests/test_arithmetic.cpp — Literals, arithmetic, logical operators
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

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

TEST_P(HeapSafety3DTest, ElementwiseDoubleAddSameShape)
{
    eval("A = ones(2, 3, 2); B = ones(2, 3, 2) * 2; C = A + B;");
    auto *C = getVarPtr("C");
    ASSERT_NE(C, nullptr);
    EXPECT_EQ(C->type(), MType::DOUBLE);
    EXPECT_TRUE(C->dims().is3D());
    EXPECT_EQ(C->numel(), 12u);
    for (size_t i = 0; i < 12; ++i)
        EXPECT_DOUBLE_EQ(C->doubleData()[i], 3.0);
}

TEST_P(HeapSafety3DTest, ElementwiseDoubleScalarOnLeft)
{
    eval("A = ones(2, 2, 2); B = 10 + A;");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_TRUE(B->dims().is3D());
    EXPECT_EQ(B->numel(), 8u);
    for (size_t i = 0; i < 8; ++i)
        EXPECT_DOUBLE_EQ(B->doubleData()[i], 11.0);
}

TEST_P(HeapSafety3DTest, ElementwiseDoubleMismatchThrows)
{
    EXPECT_THROW(
        { eval("A = ones(2, 3, 2); B = ones(3, 2, 2); C = A + B;"); },
        std::exception);
}

TEST_P(HeapSafety3DTest, ElementwiseComplexAddSameShape)
{
    eval("A = ones(2, 2, 2) + 1i; B = ones(2, 2, 2) + 2i; C = A + B;");
    auto *C = getVarPtr("C");
    ASSERT_NE(C, nullptr);
    EXPECT_TRUE(C->isComplex());
    EXPECT_TRUE(C->dims().is3D());
    EXPECT_EQ(C->numel(), 8u);
    EXPECT_DOUBLE_EQ(C->complexData()[0].real(), 2.0);
    EXPECT_DOUBLE_EQ(C->complexData()[0].imag(), 3.0);
    EXPECT_DOUBLE_EQ(C->complexData()[7].imag(), 3.0);
}

TEST_P(HeapSafety3DTest, ComparisonSameShape)
{
    eval("A = ones(2, 2, 2) * 5; B = ones(2, 2, 2) * 3; M = A > B;");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(M->type(), MType::LOGICAL);
    EXPECT_TRUE(M->dims().is3D());
    EXPECT_EQ(M->numel(), 8u);
    for (size_t i = 0; i < 8; ++i)
        EXPECT_EQ(M->logicalData()[i], 1u);
}

TEST_P(HeapSafety3DTest, ComparisonScalarOnLeft)
{
    eval("A = ones(2, 2, 2) * 4; M = 10 > A;");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(M->type(), MType::LOGICAL);
    EXPECT_TRUE(M->dims().is3D());
    EXPECT_EQ(M->numel(), 8u);
    for (size_t i = 0; i < 8; ++i)
        EXPECT_EQ(M->logicalData()[i], 1u);
}

TEST_P(HeapSafety3DTest, ComparisonMismatchThrows)
{
    EXPECT_THROW(
        { eval("A = ones(2, 3, 2); B = ones(3, 2, 2); M = A == B;"); },
        std::exception);
}

TEST_P(HeapSafety3DTest, Comparison3DVs2DMismatchThrowsCleanly)
{
    // 3D vs 2D: MATLAB broadcasts singleton page of the 2D over the
    // 3D's pages. Our impl doesn't yet do 3D broadcasting, so this
    // must throw rather than corrupt the heap or fall through to the
    // 2D broadcastDims path.
    EXPECT_THROW(
        { eval("A = ones(2, 3, 2); B = ones(2, 3); M = A == B;"); },
        std::exception);
    EXPECT_THROW(
        { eval("A = ones(2, 3); B = ones(2, 3, 2); M = A == B;"); },
        std::exception);
}

INSTANTIATE_DUAL(HeapSafety3DTest);

// ============================================================
// Empty-operand shape preservation in binary arithmetic
// ============================================================

class EmptyArith2Test : public DualEngineTest {};

TEST_P(EmptyArith2Test, DoublePlusScalarPreservesShape)
{
    eval("a = zeros(2, 0); b = a + 5;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::DOUBLE);
    EXPECT_EQ(b->dims().rows(), 2u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(EmptyArith2Test, ScalarPlusDoublePreservesShape)
{
    eval("a = zeros(3, 0); b = 5 + a;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::DOUBLE);
    EXPECT_EQ(b->dims().rows(), 3u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(EmptyArith2Test, DoublePlusComplexScalarGivesComplexEmpty)
{
    eval("a = zeros(2, 0); b = a + 0i;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->isComplex());
    EXPECT_EQ(b->dims().rows(), 2u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(EmptyArith2Test, SameShapeEmptiesPreserved)
{
    eval("a = zeros(2, 0); b = zeros(2, 0); c = a + b;");
    auto *c = getVarPtr("c");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->dims().rows(), 2u);
    EXPECT_EQ(c->dims().cols(), 0u);
}

TEST_P(EmptyArith2Test, MismatchedEmptiesThrow)
{
    EXPECT_THROW({ eval("zeros(2, 0) + zeros(0, 3);"); }, std::exception);
}

TEST_P(EmptyArith2Test, IntegerEmptyPlusScalarKeepsType)
{
    eval("a = int32(zeros(2, 0)); b = a + int32(5);");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::INT32);
    EXPECT_EQ(b->dims().rows(), 2u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(EmptyArith2Test, DotMulEmptyPreservesShape)
{
    eval("a = zeros(3, 0); b = a .* 2;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->dims().rows(), 3u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(EmptyArith2Test, DotDivEmptyPreservesShape)
{
    eval("a = zeros(2, 0); b = 10 ./ a;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->dims().rows(), 2u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(EmptyArith2Test, DotPowEmptyPreservesShape)
{
    eval("a = zeros(4, 0); b = a .^ 2;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->dims().rows(), 4u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(EmptyArith2Test, CharEmptyPlusScalarPromotesToDouble)
{
    eval("b = '' + 5;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::DOUBLE);
    EXPECT_EQ(b->numel(), 0u);
}

TEST_P(EmptyArith2Test, RightDivEmptyPreservesShape)
{
    eval("a = zeros(3, 0); b = a / 2;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->dims().rows(), 3u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(EmptyArith2Test, PowerEmptyPreservesShape)
{
    eval("a = zeros(2, 0); b = a ^ 2;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->dims().rows(), 2u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(EmptyArith2Test, LeftDivEmptyPreservesShape)
{
    eval("a = zeros(4, 0); b = 2 \\ a;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->dims().rows(), 4u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

INSTANTIATE_DUAL(EmptyArith2Test);

// ============================================================
// Matmul empty semantics (MATLAB): A(M,K) * B(K,N) = zeros(M,N)
// when the inner dim K matches (K==0 included).
// ============================================================

class MatmulEmptyTest : public DualEngineTest {};

TEST_P(MatmulEmptyTest, InnerZeroProducesAllZeros)
{
    eval("A = zeros(3, 0); B = zeros(0, 5); C = A * B;");
    auto *C = getVarPtr("C");
    ASSERT_NE(C, nullptr);
    EXPECT_EQ(C->type(), MType::DOUBLE);
    EXPECT_FALSE(C->dims().is3D());
    EXPECT_EQ(C->dims().rows(), 3u);
    EXPECT_EQ(C->dims().cols(), 5u);
    EXPECT_EQ(C->numel(), 15u);
    for (size_t i = 0; i < 15; ++i)
        EXPECT_DOUBLE_EQ(C->doubleData()[i], 0.0);
}

TEST_P(MatmulEmptyTest, RowTimesColWithZeroInner)
{
    // (3x0) * (0x0) → 3x0
    eval("A = zeros(3, 0); B = zeros(0, 0); C = A * B;");
    auto *C = getVarPtr("C");
    ASSERT_NE(C, nullptr);
    EXPECT_EQ(C->dims().rows(), 3u);
    EXPECT_EQ(C->dims().cols(), 0u);
}

TEST_P(MatmulEmptyTest, ZeroByZeroTimesZeroByN)
{
    // (0x0) * (0x5) → 0x5
    eval("A = zeros(0, 0); B = zeros(0, 5); C = A * B;");
    auto *C = getVarPtr("C");
    ASSERT_NE(C, nullptr);
    EXPECT_EQ(C->dims().rows(), 0u);
    EXPECT_EQ(C->dims().cols(), 5u);
}

TEST_P(MatmulEmptyTest, InnerMismatchThrows)
{
    EXPECT_THROW({ eval("zeros(3, 4) * zeros(0, 5);"); }, std::exception);
    EXPECT_THROW({ eval("zeros(3, 4) * zeros(5, 2);"); }, std::exception);
}

TEST_P(MatmulEmptyTest, ComplexInnerZeroProducesZeros)
{
    eval("A = complex(zeros(2, 0)); B = complex(zeros(0, 3)); C = A * B;");
    auto *C = getVarPtr("C");
    ASSERT_NE(C, nullptr);
    EXPECT_TRUE(C->isComplex());
    EXPECT_EQ(C->dims().rows(), 2u);
    EXPECT_EQ(C->dims().cols(), 3u);
    for (size_t i = 0; i < 6; ++i) {
        EXPECT_DOUBLE_EQ(C->complexData()[i].real(), 0.0);
        EXPECT_DOUBLE_EQ(C->complexData()[i].imag(), 0.0);
    }
}

TEST_P(MatmulEmptyTest, ScalarTimesEmptyStillElementwise)
{
    // scalar * empty should still delegate to elementwise path —
    // verify shape is preserved by the earlier elementwise fix.
    eval("C = 5 * zeros(3, 0);");
    auto *C = getVarPtr("C");
    ASSERT_NE(C, nullptr);
    EXPECT_EQ(C->dims().rows(), 3u);
    EXPECT_EQ(C->dims().cols(), 0u);
}

INSTANTIATE_DUAL(MatmulEmptyTest);

// ============================================================
// 3D shape/heap safety for scalar and math/complex/type helpers
// ============================================================

class HeapSafety3DExtendedTest : public DualEngineTest {};

TEST_P(HeapSafety3DExtendedTest, AbsOf3DComplex)
{
    eval("A = ones(2, 2, 2) + 1i; B = abs(A);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(B->type(), MType::DOUBLE);
    EXPECT_TRUE(B->dims().is3D());
    EXPECT_EQ(B->numel(), 8u);
    for (size_t i = 0; i < 8; ++i)
        EXPECT_NEAR(B->doubleData()[i], std::sqrt(2.0), 1e-12);
}

TEST_P(HeapSafety3DExtendedTest, RealOf3DComplex)
{
    eval("A = ones(2, 2, 2) * 3 + 4i; B = real(A);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(B->type(), MType::DOUBLE);
    EXPECT_TRUE(B->dims().is3D());
    for (size_t i = 0; i < 8; ++i)
        EXPECT_DOUBLE_EQ(B->doubleData()[i], 3.0);
}

TEST_P(HeapSafety3DExtendedTest, ImagOf3DComplex)
{
    eval("A = ones(2, 2, 2) * 3 + 4i; B = imag(A);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(B->type(), MType::DOUBLE);
    EXPECT_TRUE(B->dims().is3D());
    for (size_t i = 0; i < 8; ++i)
        EXPECT_DOUBLE_EQ(B->doubleData()[i], 4.0);
}

TEST_P(HeapSafety3DExtendedTest, IsNan3D)
{
    eval("A = zeros(2, 2, 2); A(3) = nan; M = isnan(A);");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(M->type(), MType::LOGICAL);
    EXPECT_TRUE(M->dims().is3D());
    EXPECT_EQ(M->numel(), 8u);
    EXPECT_EQ(M->logicalData()[2], 1u);
    EXPECT_EQ(M->logicalData()[0], 0u);
}

TEST_P(HeapSafety3DExtendedTest, IsInf3D)
{
    eval("A = zeros(2, 2, 2); A(5) = inf; M = isinf(A);");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(M->type(), MType::LOGICAL);
    EXPECT_TRUE(M->dims().is3D());
    EXPECT_EQ(M->logicalData()[4], 1u);
    EXPECT_EQ(M->logicalData()[0], 0u);
}

TEST_P(HeapSafety3DExtendedTest, ElementwiseTypedInt32SameShape)
{
    eval("A = int32(zeros(2, 3, 2)); for k = 1:12, A(k) = k; end");
    eval("B = int32(zeros(2, 3, 2)); for k = 1:12, B(k) = 2*k; end");
    eval("C = A + B;");
    auto *C = getVarPtr("C");
    ASSERT_NE(C, nullptr);
    EXPECT_EQ(C->type(), MType::INT32);
    EXPECT_TRUE(C->dims().is3D());
    EXPECT_EQ(C->numel(), 12u);
    for (size_t i = 0; i < 12; ++i)
        EXPECT_EQ(C->int32Data()[i], static_cast<int32_t>(3 * (i + 1)));
}

TEST_P(HeapSafety3DExtendedTest, ElementwiseTypedInt32ScalarBroadcast)
{
    eval("A = int32(zeros(2, 2, 2)); for k = 1:8, A(k) = k; end");
    eval("B = A + int32(10);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(B->type(), MType::INT32);
    EXPECT_TRUE(B->dims().is3D());
    for (size_t i = 0; i < 8; ++i)
        EXPECT_EQ(B->int32Data()[i], static_cast<int32_t>(11 + i));
}

TEST_P(HeapSafety3DExtendedTest, Polyval3D)
{
    // polyval([1 2 3], X) = X.^2 + 2*X + 3
    eval("X = ones(2, 2, 2) * 5; Y = polyval([1 2 3], X);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_EQ(Y->type(), MType::DOUBLE);
    EXPECT_TRUE(Y->dims().is3D());
    EXPECT_EQ(Y->numel(), 8u);
    // 5^2 + 2*5 + 3 = 38
    for (size_t i = 0; i < 8; ++i)
        EXPECT_DOUBLE_EQ(Y->doubleData()[i], 38.0);
}

// The DSP functions below operate linearly over numel() on 3D inputs
// (MATLAB slices by first non-singleton dim; our semantics differ).
// These tests only assert that shape + type are preserved and that
// the heap isn't corrupted. Value-level MATLAB parity for 3D DSP is a
// separate feature.

TEST_P(HeapSafety3DExtendedTest, Filter3DShapePreserved)
{
    eval("X = ones(2, 2, 2); Y = filter([1], [1], X);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_EQ(Y->type(), MType::DOUBLE);
    EXPECT_TRUE(Y->dims().is3D());
    EXPECT_EQ(Y->numel(), 8u);
}

TEST_P(HeapSafety3DExtendedTest, Filtfilt3DShapePreserved)
{
    eval("X = ones(2, 2, 2); Y = filtfilt([0.5 0.5], [1], X);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_TRUE(Y->dims().is3D());
    EXPECT_EQ(Y->numel(), 8u);
}

TEST_P(HeapSafety3DExtendedTest, Unwrap3DShapePreserved)
{
    eval("X = zeros(2, 2, 2); Y = unwrap(X);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_EQ(Y->type(), MType::DOUBLE);
    EXPECT_TRUE(Y->dims().is3D());
    EXPECT_EQ(Y->numel(), 8u);
}

TEST_P(HeapSafety3DExtendedTest, Envelope3DShapePreserved)
{
    eval("X = ones(2, 2, 2); Y = envelope(X);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_EQ(Y->type(), MType::DOUBLE);
    EXPECT_TRUE(Y->dims().is3D());
    EXPECT_EQ(Y->numel(), 8u);
}

TEST_P(HeapSafety3DExtendedTest, Fftshift3DDoubleShapePreserved)
{
    eval("X = ones(2, 2, 2); for k = 1:8, X(k) = k; end; Y = fftshift(X);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_EQ(Y->type(), MType::DOUBLE);
    EXPECT_TRUE(Y->dims().is3D());
    EXPECT_EQ(Y->numel(), 8u);
    // Linear circular shift by N/2 = 4: output[0] = input[4] = 5.
    EXPECT_DOUBLE_EQ(Y->doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(Y->doubleData()[4], 1.0);
}

TEST_P(HeapSafety3DExtendedTest, Fftshift3DComplexShapePreserved)
{
    eval("X = ones(2, 2, 2) + 1i; Y = fftshift(X);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_TRUE(Y->isComplex());
    EXPECT_TRUE(Y->dims().is3D());
    EXPECT_EQ(Y->numel(), 8u);
}

TEST_P(HeapSafety3DExtendedTest, Ifftshift3DDoubleShapePreserved)
{
    eval("X = ones(2, 2, 2); for k = 1:8, X(k) = k; end; Y = ifftshift(X);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_EQ(Y->type(), MType::DOUBLE);
    EXPECT_TRUE(Y->dims().is3D());
    EXPECT_EQ(Y->numel(), 8u);
}

TEST_P(HeapSafety3DExtendedTest, Ifftshift3DComplexShapePreserved)
{
    eval("X = ones(2, 2, 2) + 0i; Y = ifftshift(X);");
    auto *Y = getVarPtr("Y");
    ASSERT_NE(Y, nullptr);
    EXPECT_TRUE(Y->isComplex());
    EXPECT_TRUE(Y->dims().is3D());
    EXPECT_EQ(Y->numel(), 8u);
}

TEST_P(HeapSafety3DExtendedTest, Strlength3DPreservesShape)
{
    eval("S = reshape([\"aa\" \"bbb\" \"c\" \"dddd\" \"ee\" \"f\" \"ggg\" \"hh\"], 2, 2, 2);");
    eval("L = strlength(S);");
    auto *L = getVarPtr("L");
    ASSERT_NE(L, nullptr);
    EXPECT_EQ(L->type(), MType::DOUBLE);
    EXPECT_TRUE(L->dims().is3D());
    EXPECT_EQ(L->numel(), 8u);
    EXPECT_DOUBLE_EQ(L->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(L->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(L->doubleData()[2], 1.0);
    EXPECT_DOUBLE_EQ(L->doubleData()[3], 4.0);
    EXPECT_DOUBLE_EQ(L->doubleData()[7], 2.0);
}

INSTANTIATE_DUAL(HeapSafety3DExtendedTest);
