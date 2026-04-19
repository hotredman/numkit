// tests/stdlib/matrix_test.cpp — Matrix construction, bounds, 3D arrays
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

// ============================================================
// Matrix construction
// ============================================================

class MatrixTest : public DualEngineTest
{};

TEST_P(MatrixTest, RowVector)
{
    eval("v = [1 2 3];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 3u);
}

TEST_P(MatrixTest, ColumnVector)
{
    eval("v = [1; 2; 3];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 1u);
}

TEST_P(MatrixTest, Matrix2x3)
{
    eval("M = [1 2 3; 4 5 6];");
    auto *M = getVarPtr("M");
    EXPECT_EQ(rows(*M), 2u);
    EXPECT_EQ(cols(*M), 3u);
    EXPECT_DOUBLE_EQ((*M)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*M)(1, 2), 6.0);
}

TEST_P(MatrixTest, Transpose)
{
    eval("v = [1 2 3]; w = v';");
    auto *w = getVarPtr("w");
    EXPECT_EQ(rows(*w), 3u);
    EXPECT_EQ(cols(*w), 1u);
    EXPECT_DOUBLE_EQ((*w)(2, 0), 3.0);
}

TEST_P(MatrixTest, MatrixMultiply)
{
    eval("A = [1 2; 3 4]; B = [5; 6]; C = A * B;");
    auto *C = getVarPtr("C");
    EXPECT_EQ(rows(*C), 2u);
    EXPECT_EQ(cols(*C), 1u);
    EXPECT_DOUBLE_EQ((*C)(0, 0), 17.0); // 1*5+2*6
    EXPECT_DOUBLE_EQ((*C)(1, 0), 39.0); // 3*5+4*6
}

TEST_P(MatrixTest, ElementWiseMul)
{
    eval("r = [1 2 3] .* [4 5 6];");
    auto *r = getVarPtr("r");
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 10.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[2], 18.0);
}

TEST_P(MatrixTest, ElementWisePow)
{
    eval("r = [2 3] .^ [3 2];");
    auto *r = getVarPtr("r");
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 8.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 9.0);
}

TEST_P(MatrixTest, StringConcatInMatrix)
{
    eval("s = ['hello' ' ' 'world'];");
    auto *s = getVarPtr("s");
    EXPECT_EQ(s->toString(), "hello world");
}

INSTANTIATE_DUAL(MatrixTest);

// ============================================================
// Bounds checking
// ============================================================

class BoundsTest : public DualEngineTest
{};

TEST_P(BoundsTest, OutOfBoundsLinear)
{
    eval("v = [1 2 3];");
    EXPECT_THROW(eval("v(5);"), std::runtime_error);
}

TEST_P(BoundsTest, OutOfBoundsRow)
{
    eval("A = [1 2; 3 4];");
    EXPECT_THROW(eval("A(3, 1);"), std::runtime_error);
}

TEST_P(BoundsTest, OutOfBoundsCol)
{
    eval("A = [1 2; 3 4];");
    EXPECT_THROW(eval("A(1, 5);"), std::runtime_error);
}

TEST_P(BoundsTest, ValidBoundsOK)
{
    eval("v = [10 20 30];");
    EXPECT_DOUBLE_EQ(evalScalar("v(3);"), 30.0);
}

TEST_P(BoundsTest, IndexZeroError)
{
    eval("v = [1 2 3];");
    EXPECT_THROW(eval("v(0);"), std::runtime_error);
}

INSTANTIATE_DUAL(BoundsTest);

// ============================================================
// 3D array indexing
// ============================================================

class Array3DTest : public DualEngineTest
{};

TEST_P(Array3DTest, Create3DAndIndex)
{
    eval("A = zeros(2, 3, 2); A(1,1,1) = 1; A(2,3,2) = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("A(1,1,1);"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(2,3,2);"), 99.0);
}

TEST_P(Array3DTest, LinearIndexInto3D)
{
    eval("A = zeros(2, 3, 2); A(2,3,2) = 42;");
    // linear index of (2,3,2) in 2x3x2 = 2 + (3-1)*2 + (2-1)*6 = 12
    EXPECT_DOUBLE_EQ(evalScalar("A(12);"), 42.0);
}

TEST_P(Array3DTest, ScalarAssign3D)
{
    eval("A = zeros(2, 2, 2); A(1,2,2) = 77;");
    EXPECT_DOUBLE_EQ(evalScalar("A(1,2,2);"), 77.0);
    EXPECT_DOUBLE_EQ(evalScalar("A(1,1,1);"), 0.0);
}

INSTANTIATE_DUAL(Array3DTest);

// ============================================================
// reshape — 1D/2D/3D forms
// ============================================================

class ReshapeTest : public DualEngineTest {};

TEST_P(ReshapeTest, ReshapeVectorTo2DThreeScalars)
{
    eval("A = reshape(1:6, 2, 3);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 3u);
    EXPECT_FALSE(A->dims().is3D());
    EXPECT_DOUBLE_EQ(A->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[5], 6.0);
}

TEST_P(ReshapeTest, ReshapeVectorTo3DFourScalars)
{
    eval("A = reshape(1:12, 2, 3, 2);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_TRUE(A->dims().is3D());
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 3u);
    EXPECT_EQ(A->dims().pages(), 2u);
    EXPECT_EQ(A->numel(), 12u);
    EXPECT_DOUBLE_EQ(A->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[11], 12.0);
}

TEST_P(ReshapeTest, ReshapeVectorTo3DWithDimsVector)
{
    eval("A = reshape(1:12, [2 3 2]);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_TRUE(A->dims().is3D());
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 3u);
    EXPECT_EQ(A->dims().pages(), 2u);
    EXPECT_DOUBLE_EQ(A->doubleData()[11], 12.0);
}

TEST_P(ReshapeTest, ReshapeVectorTo2DWithDimsVector)
{
    eval("A = reshape(1:6, [2 3]);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 3u);
    EXPECT_FALSE(A->dims().is3D());
}

TEST_P(ReshapeTest, Reshape3DTo2D)
{
    eval("A = zeros(2, 3, 2); for k = 1:12, A(k) = k; end; B = reshape(A, 4, 3);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_FALSE(B->dims().is3D());
    EXPECT_EQ(B->dims().rows(), 4u);
    EXPECT_EQ(B->dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(B->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(B->doubleData()[11], 12.0);
}

TEST_P(ReshapeTest, ReshapePreservesType)
{
    eval("A = reshape(int32(1:12), 2, 3, 2);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_EQ(A->type(), MType::INT32);
    EXPECT_TRUE(A->dims().is3D());
    EXPECT_EQ(A->int32Data()[11], 12);
}

TEST_P(ReshapeTest, ReshapeMismatchedCountThrows)
{
    EXPECT_THROW({ eval("reshape(1:5, 2, 3);"); }, std::exception);
    EXPECT_THROW({ eval("reshape(1:12, 2, 3, 3);"); }, std::exception);
}

TEST_P(ReshapeTest, Reshape2DTo3D)
{
    eval("A = reshape(1:12, 3, 4); B = reshape(A, 2, 3, 2);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_TRUE(B->dims().is3D());
    EXPECT_EQ(B->dims().rows(), 2u);
    EXPECT_EQ(B->dims().cols(), 3u);
    EXPECT_EQ(B->dims().pages(), 2u);
    EXPECT_DOUBLE_EQ(B->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(B->doubleData()[11], 12.0);
}

TEST_P(ReshapeTest, ReshapeInfersTrailingDim)
{
    eval("A = reshape(1:12, 3, []);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_EQ(A->dims().rows(), 3u);
    EXPECT_EQ(A->dims().cols(), 4u);
    EXPECT_FALSE(A->dims().is3D());
}

TEST_P(ReshapeTest, ReshapeInfersLeadingDim)
{
    eval("A = reshape(1:12, [], 3);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_EQ(A->dims().rows(), 4u);
    EXPECT_EQ(A->dims().cols(), 3u);
}

TEST_P(ReshapeTest, ReshapeInfersMiddleDim3D)
{
    eval("A = reshape(1:12, 2, [], 2);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_TRUE(A->dims().is3D());
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 3u);
    EXPECT_EQ(A->dims().pages(), 2u);
}

TEST_P(ReshapeTest, ReshapeInfersPageDim)
{
    eval("A = reshape(1:24, 2, 3, []);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_TRUE(A->dims().is3D());
    EXPECT_EQ(A->dims().pages(), 4u);
}

TEST_P(ReshapeTest, ReshapeInfersLeadingDim3D)
{
    eval("A = reshape(1:24, [], 3, 4);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_TRUE(A->dims().is3D());
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 3u);
    EXPECT_EQ(A->dims().pages(), 4u);
    EXPECT_DOUBLE_EQ(A->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[23], 24.0);
}

TEST_P(ReshapeTest, ReshapeInferPreservesValuesAndType)
{
    eval("A = reshape(int32(1:12), 2, [], 2);");
    auto *A = getVarPtr("A");
    ASSERT_NE(A, nullptr);
    EXPECT_EQ(A->type(), MType::INT32);
    EXPECT_TRUE(A->dims().is3D());
    EXPECT_EQ(A->dims().cols(), 3u);
    EXPECT_EQ(A->int32Data()[0], 1);
    EXPECT_EQ(A->int32Data()[11], 12);
}

TEST_P(ReshapeTest, ReshapeDimsVectorRejectsEmpty)
{
    EXPECT_THROW({ eval("reshape(1:12, [3 []]);"); }, std::exception);
}

TEST_P(ReshapeTest, ReshapeTwoPlaceholdersThrows)
{
    EXPECT_THROW({ eval("reshape(1:12, [], [], 2);"); }, std::exception);
}

TEST_P(ReshapeTest, ReshapeNotDivisibleThrows)
{
    EXPECT_THROW({ eval("reshape(1:10, 3, []);"); }, std::exception);
}

TEST_P(ReshapeTest, ReshapeZeroKnownDimWithPlaceholderThrows)
{
    EXPECT_THROW({ eval("reshape(1:12, 0, []);"); }, std::exception);
}

TEST_P(ReshapeTest, ReshapeStringArrayTo3DPreservesValues)
{
    eval("S = reshape([\"aa\" \"bbb\" \"c\" \"dddd\" \"ee\" \"f\" \"ggg\" \"hh\"], 2, 2, 2);");
    auto *S = getVarPtr("S");
    ASSERT_NE(S, nullptr);
    EXPECT_EQ(S->type(), MType::STRING);
    EXPECT_TRUE(S->dims().is3D());
    EXPECT_EQ(S->numel(), 8u);
    EXPECT_EQ(S->stringElem(0), "aa");
    EXPECT_EQ(S->stringElem(3), "dddd");
    EXPECT_EQ(S->stringElem(7), "hh");
}

TEST_P(ReshapeTest, ReshapeCellTo3DPreservesValues)
{
    eval("C = reshape({1, 'ab', [3 4 5], 7, 8, 9, 10, 11}, 2, 2, 2);");
    auto *C = getVarPtr("C");
    ASSERT_NE(C, nullptr);
    EXPECT_EQ(C->type(), MType::CELL);
    EXPECT_TRUE(C->dims().is3D());
    EXPECT_EQ(C->numel(), 8u);
    EXPECT_DOUBLE_EQ(C->cellAt(0).toScalar(), 1.0);
    EXPECT_EQ(C->cellAt(1).toString(), "ab");
    EXPECT_DOUBLE_EQ(C->cellAt(7).toScalar(), 11.0);
}

TEST_P(ReshapeTest, ReshapeComplexPreservesValues)
{
    eval("A = (1:8) + (1:8) * 1i; B = reshape(A, 2, 2, 2);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_TRUE(B->isComplex());
    EXPECT_TRUE(B->dims().is3D());
    EXPECT_EQ(B->numel(), 8u);
    EXPECT_DOUBLE_EQ(B->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(B->complexData()[0].imag(), 1.0);
    EXPECT_DOUBLE_EQ(B->complexData()[7].real(), 8.0);
    EXPECT_DOUBLE_EQ(B->complexData()[7].imag(), 8.0);
}

INSTANTIATE_DUAL(ReshapeTest);

// ============================================================
// Transpose on 3D — MATLAB errors ("not defined for N-D arrays")
// ============================================================

class Transpose3DTest : public DualEngineTest {};

TEST_P(Transpose3DTest, ConjugateTransposeOn3DThrows)
{
    EXPECT_THROW({ eval("A = ones(2, 3, 2); B = A';"); }, std::exception);
}

TEST_P(Transpose3DTest, NonConjugateTransposeOn3DThrows)
{
    EXPECT_THROW({ eval("A = ones(2, 3, 2); B = A.';"); }, std::exception);
}

TEST_P(Transpose3DTest, TransposeFunctionOn3DThrows)
{
    EXPECT_THROW({ eval("A = ones(2, 3, 2); B = transpose(A);"); }, std::exception);
}

TEST_P(Transpose3DTest, ComplexTransposeOn3DThrows)
{
    EXPECT_THROW({ eval("A = ones(2, 3, 2) + 1i; B = A';"); }, std::exception);
}

INSTANTIATE_DUAL(Transpose3DTest);

// ============================================================
// Reductions on 3D — reduce along first non-singleton dim
// ============================================================

class Reductions3DTest : public DualEngineTest {};

TEST_P(Reductions3DTest, SumAlongRows3D)
{
    // 2×3×2: sum along rows → 1×3×2.
    eval("A = reshape(1:12, 2, 3, 2); S = sum(A);");
    auto *S = getVarPtr("S");
    ASSERT_NE(S, nullptr);
    EXPECT_TRUE(S->dims().is3D());
    EXPECT_EQ(S->dims().rows(), 1u);
    EXPECT_EQ(S->dims().cols(), 3u);
    EXPECT_EQ(S->dims().pages(), 2u);
    // Page 1 cols: 1+2=3, 3+4=7, 5+6=11.
    EXPECT_DOUBLE_EQ(S->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[1], 7.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[2], 11.0);
    // Page 2 cols: 7+8=15, 9+10=19, 11+12=23.
    EXPECT_DOUBLE_EQ(S->doubleData()[3], 15.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[5], 23.0);
}

TEST_P(Reductions3DTest, MaxAlongRows3D)
{
    eval("A = reshape(1:12, 2, 3, 2); M = max(A);");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_TRUE(M->dims().is3D());
    EXPECT_EQ(M->numel(), 6u);
    // per-column max on each page: page1 = [2 4 6], page2 = [8 10 12]
    EXPECT_DOUBLE_EQ(M->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(M->doubleData()[2], 6.0);
    EXPECT_DOUBLE_EQ(M->doubleData()[3], 8.0);
    EXPECT_DOUBLE_EQ(M->doubleData()[5], 12.0);
}

TEST_P(Reductions3DTest, MinAlongRows3D)
{
    eval("A = reshape(1:12, 2, 3, 2); M = min(A);");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_TRUE(M->dims().is3D());
    EXPECT_DOUBLE_EQ(M->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(M->doubleData()[5], 11.0);
}

TEST_P(Reductions3DTest, ProdAlongRows3D)
{
    eval("A = reshape(1:8, 2, 2, 2); P = prod(A);");
    auto *P = getVarPtr("P");
    ASSERT_NE(P, nullptr);
    EXPECT_TRUE(P->dims().is3D());
    // per-column prod: page1 = [1*2 3*4] = [2 12], page2 = [5*6 7*8] = [30 56]
    EXPECT_DOUBLE_EQ(P->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(P->doubleData()[1], 12.0);
    EXPECT_DOUBLE_EQ(P->doubleData()[2], 30.0);
    EXPECT_DOUBLE_EQ(P->doubleData()[3], 56.0);
}

TEST_P(Reductions3DTest, MeanAlongRows3D)
{
    eval("A = reshape(1:12, 2, 3, 2); M = mean(A);");
    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_TRUE(M->dims().is3D());
    // per-column mean: page1 = [1.5 3.5 5.5], page2 = [7.5 9.5 11.5]
    EXPECT_DOUBLE_EQ(M->doubleData()[0], 1.5);
    EXPECT_DOUBLE_EQ(M->doubleData()[2], 5.5);
    EXPECT_DOUBLE_EQ(M->doubleData()[5], 11.5);
}

TEST_P(Reductions3DTest, SumAlongColsWhenRowsSingleton)
{
    // 1x3x2: first non-singleton is dim 2 (cols). Reduce → 1x1x2.
    eval("A = reshape(1:6, 1, 3, 2); S = sum(A);");
    auto *S = getVarPtr("S");
    ASSERT_NE(S, nullptr);
    EXPECT_TRUE(S->dims().is3D());
    EXPECT_EQ(S->dims().rows(), 1u);
    EXPECT_EQ(S->dims().cols(), 1u);
    EXPECT_EQ(S->dims().pages(), 2u);
    EXPECT_DOUBLE_EQ(S->doubleData()[0], 6.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[1], 15.0);
}

TEST_P(Reductions3DTest, Max3DReturnsIndices)
{
    eval("A = reshape(1:12, 2, 3, 2); [V, I] = max(A);");
    auto *V = getVarPtr("V");
    auto *I = getVarPtr("I");
    ASSERT_NE(V, nullptr);
    ASSERT_NE(I, nullptr);
    EXPECT_TRUE(I->dims().is3D());
    EXPECT_EQ(I->numel(), 6u);
    // max is always in row 2 → idx = 2 everywhere.
    for (size_t i = 0; i < 6; ++i)
        EXPECT_DOUBLE_EQ(I->doubleData()[i], 2.0);
}

INSTANTIATE_DUAL(Reductions3DTest);

