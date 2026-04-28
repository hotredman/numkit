// tests/stdlib/matrix_test.cpp — Matrix construction, bounds, 3D arrays
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace m_test;

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

// ============================================================
// sort / find — MATLAB shape semantics (2D and 3D)
// ============================================================

class SortFindTest : public DualEngineTest {};

TEST_P(SortFindTest, SortRowVectorKeepsShape)
{
    eval("y = sort([3 1 2]);");
    auto *y = getVarPtr("y");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(y->dims().rows(), 1u);
    EXPECT_EQ(y->dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(y->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(y->doubleData()[2], 3.0);
}

TEST_P(SortFindTest, SortColVectorKeepsShape)
{
    eval("y = sort([3; 1; 2]);");
    auto *y = getVarPtr("y");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(y->dims().rows(), 3u);
    EXPECT_EQ(y->dims().cols(), 1u);
    EXPECT_DOUBLE_EQ(y->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(y->doubleData()[2], 3.0);
}

TEST_P(SortFindTest, SortMatrixPerColumn)
{
    // MATLAB: sort(A) sorts each column of a 2D matrix.
    eval("y = sort([3 5; 1 2; 2 4]);");
    auto *y = getVarPtr("y");
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(y->dims().rows(), 3u);
    EXPECT_EQ(y->dims().cols(), 2u);
    // Col 1 sorted: 1, 2, 3; col 2 sorted: 2, 4, 5.
    EXPECT_DOUBLE_EQ(y->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(y->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(y->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(y->doubleData()[3], 2.0);
    EXPECT_DOUBLE_EQ(y->doubleData()[5], 5.0);
}

TEST_P(SortFindTest, Sort3DPerColumnPerPage)
{
    eval("A = reshape([3 1 5 2  7 4 8 6], 2, 2, 2); B = sort(A);");
    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_TRUE(B->dims().is3D());
    EXPECT_EQ(B->numel(), 8u);
    // page 1 cols: [3,1] -> [1,3]; [5,2] -> [2,5]
    // page 2 cols: [7,4] -> [4,7]; [8,6] -> [6,8]
    EXPECT_DOUBLE_EQ(B->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(B->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(B->doubleData()[2], 2.0);
    EXPECT_DOUBLE_EQ(B->doubleData()[3], 5.0);
    EXPECT_DOUBLE_EQ(B->doubleData()[4], 4.0);
    EXPECT_DOUBLE_EQ(B->doubleData()[7], 8.0);
}

TEST_P(SortFindTest, SortReturnsIndices)
{
    eval("[y, i] = sort([30 10 20]);");
    auto *i = getVarPtr("i");
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->numel(), 3u);
    EXPECT_DOUBLE_EQ(i->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(i->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(i->doubleData()[2], 1.0);
}

TEST_P(SortFindTest, FindRowVectorReturnsRow)
{
    eval("ix = find([0 1 0 2 0]);");
    auto *ix = getVarPtr("ix");
    ASSERT_NE(ix, nullptr);
    EXPECT_EQ(ix->dims().rows(), 1u);
    EXPECT_EQ(ix->dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[1], 4.0);
}

TEST_P(SortFindTest, FindColVectorReturnsCol)
{
    eval("ix = find([0; 1; 0; 2]);");
    auto *ix = getVarPtr("ix");
    ASSERT_NE(ix, nullptr);
    EXPECT_EQ(ix->dims().rows(), 2u);
    EXPECT_EQ(ix->dims().cols(), 1u);
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[1], 4.0);
}

TEST_P(SortFindTest, FindMatrixReturnsCol)
{
    // 2x3 with non-zeros at linear indices 2, 3, 6.
    eval("A = [0 0 0; 1 2 0]; A(6) = 9; ix = find(A);");
    auto *ix = getVarPtr("ix");
    ASSERT_NE(ix, nullptr);
    EXPECT_EQ(ix->dims().rows(), 3u);
    EXPECT_EQ(ix->dims().cols(), 1u);
}

TEST_P(SortFindTest, Find3DReturnsCol)
{
    eval("A = zeros(2, 2, 2); A(3) = 1; A(7) = 1; ix = find(A);");
    auto *ix = getVarPtr("ix");
    ASSERT_NE(ix, nullptr);
    EXPECT_EQ(ix->dims().rows(), 2u);
    EXPECT_EQ(ix->dims().cols(), 1u);
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[1], 7.0);
}

// ── sortrows ────────────────────────────────────────────────────

TEST_P(SortFindTest, SortrowsAscByAllColumns)
{
    eval("S = sortrows([3 1 2; 1 5 9; 2 6 5; 1 3 4]);");
    auto *S = getVarPtr("S");
    EXPECT_EQ(rows(*S), 4u);
    EXPECT_EQ(cols(*S), 3u);
    // Expected order: rows where first col ascending; ties broken by col 2.
    // [1 3 4], [1 5 9], [2 6 5], [3 1 2].
    EXPECT_DOUBLE_EQ((*S)(0, 0), 1.0); EXPECT_DOUBLE_EQ((*S)(0, 1), 3.0); EXPECT_DOUBLE_EQ((*S)(0, 2), 4.0);
    EXPECT_DOUBLE_EQ((*S)(1, 0), 1.0); EXPECT_DOUBLE_EQ((*S)(1, 1), 5.0); EXPECT_DOUBLE_EQ((*S)(1, 2), 9.0);
    EXPECT_DOUBLE_EQ((*S)(2, 0), 2.0); EXPECT_DOUBLE_EQ((*S)(2, 1), 6.0); EXPECT_DOUBLE_EQ((*S)(2, 2), 5.0);
    EXPECT_DOUBLE_EQ((*S)(3, 0), 3.0); EXPECT_DOUBLE_EQ((*S)(3, 1), 1.0); EXPECT_DOUBLE_EQ((*S)(3, 2), 2.0);
}

TEST_P(SortFindTest, SortrowsReturnsIndices)
{
    eval("[S, ix] = sortrows([3 1; 1 5; 2 6; 1 3]);");
    auto *ix = getVarPtr("ix");
    EXPECT_EQ(ix->numel(), 4u);
    // Original order: row1=[3 1], row2=[1 5], row3=[2 6], row4=[1 3].
    // Sorted: row4 [1 3], row2 [1 5], row3 [2 6], row1 [3 1] → idx [4 2 3 1].
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[3], 1.0);
}

TEST_P(SortFindTest, SortrowsByExplicitColumn)
{
    // Sort by column 2 only.
    eval("S = sortrows([3 1 2; 1 5 9; 2 6 5; 1 3 4], 2);");
    auto *S = getVarPtr("S");
    // col2 ascending: 1, 3, 5, 6 → rows [3 1 2], [1 3 4], [1 5 9], [2 6 5].
    EXPECT_DOUBLE_EQ((*S)(0, 1), 1.0);
    EXPECT_DOUBLE_EQ((*S)(1, 1), 3.0);
    EXPECT_DOUBLE_EQ((*S)(2, 1), 5.0);
    EXPECT_DOUBLE_EQ((*S)(3, 1), 6.0);
}

TEST_P(SortFindTest, SortrowsByNegativeColumnDescending)
{
    eval("S = sortrows([3 1; 1 5; 2 6; 1 3], -1);");
    auto *S = getVarPtr("S");
    // col1 descending: 3, 2, 1, 1 → first row [3 1], then [2 6], then ties.
    EXPECT_DOUBLE_EQ((*S)(0, 0), 3.0);
    EXPECT_DOUBLE_EQ((*S)(1, 0), 2.0);
    EXPECT_DOUBLE_EQ((*S)(2, 0), 1.0);
    EXPECT_DOUBLE_EQ((*S)(3, 0), 1.0);
}

TEST_P(SortFindTest, SortrowsMultiKeyMixedDirections)
{
    // Sort by col1 ascending, then col2 descending: -2 means desc on col2.
    eval("S = sortrows([1 5; 2 1; 1 9; 2 8], [1, -2]);");
    auto *S = getVarPtr("S");
    // col1 asc: rows {[1 5], [1 9]} then {[2 1], [2 8]}; tie-break col2 desc.
    // → [1 9], [1 5], [2 8], [2 1].
    EXPECT_DOUBLE_EQ((*S)(0, 0), 1.0); EXPECT_DOUBLE_EQ((*S)(0, 1), 9.0);
    EXPECT_DOUBLE_EQ((*S)(1, 0), 1.0); EXPECT_DOUBLE_EQ((*S)(1, 1), 5.0);
    EXPECT_DOUBLE_EQ((*S)(2, 0), 2.0); EXPECT_DOUBLE_EQ((*S)(2, 1), 8.0);
    EXPECT_DOUBLE_EQ((*S)(3, 0), 2.0); EXPECT_DOUBLE_EQ((*S)(3, 1), 1.0);
}

TEST_P(SortFindTest, SortrowsStableForTies)
{
    // All rows equal except for col2; tie on col1 should preserve original order.
    eval("[S, ix] = sortrows([1 5; 1 3; 1 9; 1 1], 1);");
    auto *ix = getVarPtr("ix");
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[3], 4.0);
}

TEST_P(SortFindTest, SortrowsColumnVector)
{
    // Column vector — single column → sort ascending.
    eval("[S, ix] = sortrows([3; 1; 2; 1]);");
    auto *S = getVarPtr("S");
    auto *ix = getVarPtr("ix");
    EXPECT_EQ(rows(*S), 4u);
    EXPECT_EQ(cols(*S), 1u);
    EXPECT_DOUBLE_EQ(S->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[2], 2.0);
    EXPECT_DOUBLE_EQ(S->doubleData()[3], 3.0);
    // Stable: original [3 1 2 1] → sorted [1(2), 1(4), 2(3), 3(1)].
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(ix->doubleData()[1], 4.0);
}

TEST_P(SortFindTest, SortrowsRowVectorPassThrough)
{
    // Single-row matrix: nothing to reorder, returns unchanged.
    eval("[S, ix] = sortrows([7 2 9 1]);");
    auto *S = getVarPtr("S");
    auto *ix = getVarPtr("ix");
    EXPECT_EQ(rows(*S), 1u);
    EXPECT_EQ(cols(*S), 4u);
    EXPECT_DOUBLE_EQ((*S)(0, 0), 7.0);
    EXPECT_DOUBLE_EQ((*S)(0, 3), 1.0);
    EXPECT_EQ(ix->numel(), 1u);
    EXPECT_DOUBLE_EQ(ix->doubleData()[0], 1.0);
}

TEST_P(SortFindTest, SortrowsEmptyMatrixReturnsEmpty)
{
    eval("[S, ix] = sortrows(zeros(0, 3));");
    auto *S = getVarPtr("S");
    auto *ix = getVarPtr("ix");
    EXPECT_EQ(rows(*S), 0u);
    EXPECT_EQ(cols(*S), 3u);
    EXPECT_EQ(ix->numel(), 0u);
}

TEST_P(SortFindTest, SortrowsNaNGoesToBottom)
{
    eval("S = sortrows([2 1; NaN 3; 1 5]);");
    auto *S = getVarPtr("S");
    // [1 5], [2 1], [NaN 3]
    EXPECT_DOUBLE_EQ((*S)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*S)(1, 0), 2.0);
    EXPECT_TRUE(std::isnan((*S)(2, 0)));
}

TEST_P(SortFindTest, SortrowsPromotesIntegerToDouble)
{
    eval("S = sortrows(int32([3 1; 1 5; 2 6]));");
    auto *S = getVarPtr("S");
    EXPECT_EQ(S->type(), MType::DOUBLE);
    EXPECT_DOUBLE_EQ((*S)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*S)(1, 0), 2.0);
    EXPECT_DOUBLE_EQ((*S)(2, 0), 3.0);
}

TEST_P(SortFindTest, SortrowsBadColumnThrows)
{
    eval("M = [1 2; 3 4];");
    EXPECT_THROW(eval("S = sortrows(M, 5);"), std::exception);
}

TEST_P(SortFindTest, SortrowsZeroColumnThrows)
{
    eval("M = [1 2; 3 4];");
    EXPECT_THROW(eval("S = sortrows(M, 0);"), std::exception);
}

TEST_P(SortFindTest, SortrowsNonIntegerColumnThrows)
{
    eval("M = [1 2; 3 4];");
    EXPECT_THROW(eval("S = sortrows(M, 1.5);"), std::exception);
}

TEST_P(SortFindTest, Sortrows3DInputThrows)
{
    eval("A = zeros(2, 2, 2);");
    EXPECT_THROW(eval("S = sortrows(A);"), std::exception);
}

// ── nnz ─────────────────────────────────────────────────────────

TEST_P(SortFindTest, NnzVector)
{
    EXPECT_DOUBLE_EQ(evalScalar("nnz([0 1 0 2 0 3]);"), 3.0);
}

TEST_P(SortFindTest, NnzMatrix)
{
    EXPECT_DOUBLE_EQ(evalScalar("nnz([1 0 2; 0 3 0; 4 0 5]);"), 5.0);
}

TEST_P(SortFindTest, NnzAllZerosIsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("nnz(zeros(3, 4));"), 0.0);
}

TEST_P(SortFindTest, NnzAllNonzero)
{
    EXPECT_DOUBLE_EQ(evalScalar("nnz(ones(2, 3));"), 6.0);
}

TEST_P(SortFindTest, NnzNanCountsAsNonzero)
{
    // NaN != 0, so it counts.
    EXPECT_DOUBLE_EQ(evalScalar("nnz([0 NaN 0 NaN]);"), 2.0);
}

TEST_P(SortFindTest, NnzLogical)
{
    EXPECT_DOUBLE_EQ(evalScalar("nnz([true false true true false]);"), 3.0);
}

TEST_P(SortFindTest, NnzInteger)
{
    EXPECT_DOUBLE_EQ(evalScalar("nnz(int32([0 -1 0 5 0 7]));"), 3.0);
}

TEST_P(SortFindTest, NnzComplexBothPartsZero)
{
    // 0+0i is zero; everything else (real-only or imag-only) counts.
    EXPECT_DOUBLE_EQ(evalScalar("nnz([0+0i, 1+0i, 0+2i, 3+4i]);"), 3.0);
}

TEST_P(SortFindTest, Nnz3D)
{
    eval("A = zeros(2, 2, 2); A(1) = 1; A(4) = 2; A(8) = 3;");
    EXPECT_DOUBLE_EQ(evalScalar("nnz(A);"), 3.0);
}

TEST_P(SortFindTest, NnzEmptyIsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("nnz([]);"), 0.0);
}

// ── nonzeros ────────────────────────────────────────────────────

TEST_P(SortFindTest, NonzerosRowVectorReturnsCol)
{
    // Returns column vector regardless of input orientation.
    eval("v = nonzeros([0 1 0 2 0 3]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 1u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 3.0);
}

TEST_P(SortFindTest, NonzerosMatrixColumnMajorOrder)
{
    // [1 0 5; 0 3 0; 2 0 6] in column-major: col0=[1,0,2], col1=[0,3,0],
    // col2=[5,0,6]. Non-zeros in column-major: 1, 2, 3, 5, 6.
    eval("v = nonzeros([1 0 5; 0 3 0; 2 0 6]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 5u);
    EXPECT_EQ(cols(*v), 1u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 5.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 6.0);
}

TEST_P(SortFindTest, NonzerosNoneReturnsEmpty)
{
    eval("v = nonzeros(zeros(3, 4));");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 0u);
    EXPECT_EQ(rows(*v), 0u);
    EXPECT_EQ(cols(*v), 1u);
}

TEST_P(SortFindTest, NonzerosPreservesIntegerType)
{
    eval("v = nonzeros(int32([0 -1 0 5 0 7]));");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), MType::INT32);
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_EQ(v->int32Data()[0], -1);
    EXPECT_EQ(v->int32Data()[1], 5);
    EXPECT_EQ(v->int32Data()[2], 7);
}

TEST_P(SortFindTest, NonzerosPreservesSingleType)
{
    eval("v = nonzeros(single([0 1.5 0 -2.5]));");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), MType::SINGLE);
    EXPECT_EQ(v->numel(), 2u);
    EXPECT_FLOAT_EQ(v->singleData()[0], 1.5f);
    EXPECT_FLOAT_EQ(v->singleData()[1], -2.5f);
}

TEST_P(SortFindTest, NonzerosPreservesLogicalType)
{
    eval("v = nonzeros([true false true true false]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), MType::LOGICAL);
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_NE(v->logicalData()[0], 0);
    EXPECT_NE(v->logicalData()[1], 0);
    EXPECT_NE(v->logicalData()[2], 0);
}

TEST_P(SortFindTest, NonzerosComplex)
{
    // 0+0i excluded; 1+0i, 0+2i, 3+4i included.
    eval("v = nonzeros([0+0i, 1+0i, 0+2i, 3+4i]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), MType::COMPLEX);
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(v->complexData()[0].imag(), 0.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].real(), 0.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].imag(), 2.0);
    EXPECT_DOUBLE_EQ(v->complexData()[2].real(), 3.0);
    EXPECT_DOUBLE_EQ(v->complexData()[2].imag(), 4.0);
}

TEST_P(SortFindTest, NonzerosNanCountsAsNonzero)
{
    eval("v = nonzeros([0 NaN 0 1 NaN]);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_TRUE(std::isnan(v->doubleData()[0]));
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 1.0);
    EXPECT_TRUE(std::isnan(v->doubleData()[2]));
}

TEST_P(SortFindTest, Nonzeros3DColumnMajor)
{
    eval("A = zeros(2, 2, 2); A(2) = 10; A(5) = 20; A(8) = 30;"
         "v = nonzeros(A);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 10.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 20.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 30.0);
}

INSTANTIATE_DUAL(SortFindTest);

// ============================================================
// ND foundation (Phase 3a.1) — ones/zeros/reshape/size for 4D+
// ============================================================

class NDFoundationTest : public DualEngineTest {};

TEST_P(NDFoundationTest, Ones4DSize)
{
    eval("A = ones([2, 3, 4, 5]); s = size(A);");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(cols(*s), 4u);
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[2], 4.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[3], 5.0);
}

TEST_P(NDFoundationTest, Ones4DNumel)
{
    EXPECT_DOUBLE_EQ(evalScalar("numel(ones([2, 3, 4, 5]));"), 120.0);
}

TEST_P(NDFoundationTest, Ones4DNdims)
{
    EXPECT_DOUBLE_EQ(evalScalar("ndims(ones([2, 3, 4, 5]));"), 4.0);
}

TEST_P(NDFoundationTest, Zeros5DSize)
{
    // 5D trips the heap path in SBO Dims (kInlineCap = 4).
    eval("A = zeros([2, 3, 4, 5, 6]); s = size(A);");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(cols(*s), 5u);
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[4], 6.0);
}

TEST_P(NDFoundationTest, Zeros5DNumel)
{
    EXPECT_DOUBLE_EQ(evalScalar("numel(zeros([2, 3, 4, 5, 6]));"), 720.0);
}

TEST_P(NDFoundationTest, ReshapeTo4DRoundTrip)
{
    eval("A = reshape(1:120, [2, 3, 4, 5]); s = size(A);");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(cols(*s), 4u);
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[3], 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A);"), 120.0);
}

TEST_P(NDFoundationTest, ReshapeTo5DRoundTrip)
{
    eval("A = reshape(1:720, [2, 3, 4, 5, 6]); s = size(A);");
    EXPECT_EQ(cols(*getVarPtr("s")), 5u);
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A);"), 720.0);
}

TEST_P(NDFoundationTest, OnesScalarArgsForm4D)
{
    // Multi-scalar form: ones(2, 3, 4, 5) — same as ones([2 3 4 5]).
    eval("A = ones(2, 3, 4, 5); s = size(A);");
    EXPECT_EQ(cols(*getVarPtr("s")), 4u);
    EXPECT_DOUBLE_EQ(evalScalar("numel(A);"), 120.0);
}

TEST_P(NDFoundationTest, ReshapeStripsTrailingOnes)
{
    // reshape(1:6, [2 3 1 1]) collapses trailing 1s → 2D matrix.
    eval("A = reshape(1:6, [2, 3, 1, 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(A);"), 2.0);
}

TEST_P(NDFoundationTest, SizeWithDimArg4D)
{
    eval("A = ones([2, 3, 4, 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 2);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 3);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 4);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(A, 5);"), 1.0);  // past ndim → 1
}

INSTANTIATE_DUAL(NDFoundationTest);

// ── pagemtimes — ND batched matmul (Polish round-2 item 7) ─────────

class PagemtimesTest : public DualEngineTest
{};

TEST_P(PagemtimesTest, Pure2DMatchesMtimes)
{
    eval("A = reshape(1:6, [2, 3]); B = reshape(1:12, [3, 4]);");
    eval("Z = pagemtimes(A, B);");
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 2);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("ndims(Z);"),   2.0);
    eval("R = A * B;");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(Z(:) - R(:)));"), 0.0);
}

TEST_P(PagemtimesTest, Pages3DSameBatch)
{
    // X: 2×3×4 batch, Y: 3×5×4 batch → Z: 2×5×4
    eval("X = reshape(1:24, [2, 3, 4]); Y = reshape(1:60, [3, 5, 4]);");
    eval("Z = pagemtimes(X, Y);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(Z);"),   3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 2);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 3);"), 4.0);
    eval("Xp = X(:,:,2); Yp = Y(:,:,2); Rp = Xp * Yp;");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(reshape(Z(:,:,2) - Rp, 1, [])));"), 0.0);
}

TEST_P(PagemtimesTest, BroadcastOneOperand2D)
{
    // 2D X (1 page) broadcast across 3D Y batch.
    eval("X = reshape(1:6, [2, 3]); Y = reshape(1:24, [3, 2, 4]);");
    eval("Z = pagemtimes(X, Y);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(Z);"),   3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 3);"), 4.0);
    eval("Yp = Y(:,:,3); Rp = X * Yp;");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(reshape(Z(:,:,3) - Rp, 1, [])));"), 0.0);
}

TEST_P(PagemtimesTest, Broadcast4DBatch)
{
    // X: [m=2,k=3,3,1], Y: [k=3,n=4,1,2] → Z: [2,4,3,2]
    eval("X = reshape(1:18, [2, 3, 3, 1]); Y = reshape(1:24, [3, 4, 1, 2]);");
    eval("Z = pagemtimes(X, Y);");
    EXPECT_DOUBLE_EQ(evalScalar("ndims(Z);"),   4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 2);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 3);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 4);"), 2.0);
    eval("Xp = X(:,:,2,1); Yp = Y(:,:,1,1); Rp = Xp * Yp;");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(reshape(Z(:,:,2,1) - Rp, 1, [])));"), 0.0);
    eval("Xp = X(:,:,3,1); Yp = Y(:,:,1,2); Rp = Xp * Yp;");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(reshape(Z(:,:,3,2) - Rp, 1, [])));"), 0.0);
}

TEST_P(PagemtimesTest, InnerDimMismatchThrows)
{
    eval("X = ones(2, 3); Y = ones(4, 5);");
    EXPECT_THROW(eval("Z = pagemtimes(X, Y);"), std::exception);
}

// ── Phase 2: transpose flags (4-arg form) ────────────────────────

TEST_P(PagemtimesTest, TransposeXNone2D)
{
    // X' * Y where X: 3×2, Y: 3×4 → result 2×4
    eval("X = reshape(1:6, [3, 2]); Y = reshape(1:12, [3, 4]);");
    eval("Z = pagemtimes(X, 'transpose', Y, 'none');");
    eval("R = X' * Y;");
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 2);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(Z(:) - R(:)));"), 0.0);
}

TEST_P(PagemtimesTest, NoneXTransposeY2D)
{
    // X * Y' where X: 2×3, Y: 4×3 → result 2×4
    eval("X = reshape(1:6, [2, 3]); Y = reshape(1:12, [4, 3]);");
    eval("Z = pagemtimes(X, 'none', Y, 'transpose');");
    eval("R = X * Y';");
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 2);"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(Z(:) - R(:)));"), 0.0);
}

TEST_P(PagemtimesTest, TransposeBoth2D)
{
    // X' * Y' where X: 3×2, Y: 4×3 → result 2×4
    eval("X = reshape(1:6, [3, 2]); Y = reshape(1:12, [4, 3]);");
    eval("Z = pagemtimes(X, 'transpose', Y, 'transpose');");
    eval("R = X' * Y';");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(Z(:) - R(:)));"), 0.0);
}

TEST_P(PagemtimesTest, TransposeBatched3D)
{
    // 3-D batched: X^T per page * Y per page.
    eval("X = reshape(1:24, [3, 2, 4]); Y = reshape(1:60, [3, 5, 4]);");
    eval("Z = pagemtimes(X, 'transpose', Y, 'none');");
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 2);"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 3);"), 4.0);
    eval("Xp = X(:,:,3); Yp = Y(:,:,3); Rp = Xp' * Yp;");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(reshape(Z(:,:,3) - Rp, 1, [])));"), 0.0);
}

TEST_P(PagemtimesTest, CTransposeMatchesTransposeForReal)
{
    // For real-valued input 'ctranspose' is identical to 'transpose'.
    eval("X = reshape(1:6, [3, 2]); Y = reshape(1:12, [3, 4]);");
    eval("Z1 = pagemtimes(X, 'transpose',  Y, 'none');");
    eval("Z2 = pagemtimes(X, 'ctranspose', Y, 'none');");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(Z1(:) - Z2(:)));"), 0.0);
}

TEST_P(PagemtimesTest, NoneNoneEqualsTwoArgForm)
{
    eval("X = reshape(1:6, [2, 3]); Y = reshape(1:12, [3, 4]);");
    eval("Z1 = pagemtimes(X, Y);");
    eval("Z2 = pagemtimes(X, 'none', Y, 'none');");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(Z1(:) - Z2(:)));"), 0.0);
}

TEST_P(PagemtimesTest, InvalidFlagThrows)
{
    eval("X = ones(2, 3); Y = ones(3, 4);");
    EXPECT_THROW(eval("Z = pagemtimes(X, 'foo', Y, 'none');"), std::exception);
}

TEST_P(PagemtimesTest, BadArgCountThrows)
{
    eval("X = ones(2, 3); Y = ones(3, 4);");
    EXPECT_THROW(eval("Z = pagemtimes(X);"), std::exception);
    EXPECT_THROW(eval("Z = pagemtimes(X, 'transpose', Y);"), std::exception);
}

TEST_P(PagemtimesTest, InnerDimMismatchUnderTranspose)
{
    // Under transpose: X is 3×2 → X' is 2×3, Y is 4×5 → inner = 3 vs 4 → throw.
    eval("X = ones(3, 2); Y = ones(4, 5);");
    EXPECT_THROW(eval("Z = pagemtimes(X, 'transpose', Y, 'none');"), std::exception);
}

// ── Phase 3: SINGLE-precision input ──────────────────────────────

TEST_P(PagemtimesTest, SingleSingle2D)
{
    eval("X = single(reshape(1:6, [2, 3])); Y = single(reshape(1:12, [3, 4]));");
    eval("Z = pagemtimes(X, Y);");
    // Expected: same value as double matmul, but stored as single.
    eval("Xd = double(X); Yd = double(Y); Rd = Xd * Yd;");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(double(Z(:)) - Rd(:)));"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("issingle(Z);"), 1.0);
}

TEST_P(PagemtimesTest, SingleDoubleMixedReturnsSingle)
{
    eval("X = single(reshape(1:6, [2, 3])); Y = reshape(1:12, [3, 4]);");
    eval("Z = pagemtimes(X, Y);");
    EXPECT_DOUBLE_EQ(evalScalar("issingle(Z);"), 1.0);
    eval("Rd = double(X) * Y;");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(double(Z(:)) - Rd(:)));"), 0.0);
}

TEST_P(PagemtimesTest, DoubleSingleMixedReturnsSingle)
{
    eval("X = reshape(1:6, [2, 3]); Y = single(reshape(1:12, [3, 4]));");
    eval("Z = pagemtimes(X, Y);");
    EXPECT_DOUBLE_EQ(evalScalar("issingle(Z);"), 1.0);
    eval("Rd = X * double(Y);");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(double(Z(:)) - Rd(:)));"), 0.0);
}

TEST_P(PagemtimesTest, SingleBatched3D)
{
    eval("X = single(reshape(1:24, [2, 3, 4])); Y = single(reshape(1:60, [3, 5, 4]));");
    eval("Z = pagemtimes(X, Y);");
    EXPECT_DOUBLE_EQ(evalScalar("issingle(Z);"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 1);"),     2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Z, 3);"),     4.0);
    eval("Xp = double(X(:,:,2)); Yp = double(Y(:,:,2)); Rp = Xp * Yp;");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(double(reshape(Z(:,:,2), 1, [])) - reshape(Rp, 1, [])));"), 0.0);
}

TEST_P(PagemtimesTest, SingleWithTranspose)
{
    // Transpose flag combined with SINGLE input.
    eval("X = single(reshape(1:6, [3, 2])); Y = single(reshape(1:12, [3, 4]));");
    eval("Z = pagemtimes(X, 'transpose', Y, 'none');");
    EXPECT_DOUBLE_EQ(evalScalar("issingle(Z);"), 1.0);
    eval("Rd = double(X)' * double(Y);");
    EXPECT_DOUBLE_EQ(evalScalar("max(abs(double(Z(:)) - Rd(:)));"), 0.0);
}

TEST_P(PagemtimesTest, IntegerInputThrows)
{
    eval("X = int32(ones(2, 3)); Y = int32(ones(3, 4));");
    EXPECT_THROW(eval("Z = pagemtimes(X, Y);"), std::exception);
}

TEST_P(PagemtimesTest, LogicalInputThrows)
{
    eval("X = logical(ones(2, 3)); Y = logical(ones(3, 4));");
    EXPECT_THROW(eval("Z = pagemtimes(X, Y);"), std::exception);
}

// ── Round 4 Item 3: COMPLEX pagemtimes ────────────────────────────

TEST_P(PagemtimesTest, ComplexComplex2D)
{
    // (1+2i) * (3+4i) = -5+10i; (1+2i)*(5+6i) = -7+16i; etc.
    // Manually verify against equivalent matmul.
    eval("X = [1+2i, 3+4i; 5+0i, 0+1i];"        // 2x2
         "Y = [1+0i, 0+1i; 2+1i, 1+0i];"        // 2x2
         "Z = pagemtimes(X, Y);"
         "Zref = X * Y;"                         // mtimes, our reference
         "diff = max(abs(reshape(Z - Zref, 1, [])));");
    EXPECT_LT(evalScalar("diff;"), 1e-12);
}

TEST_P(PagemtimesTest, RealPromotedToComplex)
{
    // Mixed real / complex → output is COMPLEX.
    eval("X = [1, 2; 3, 4];"
         "Y = [1+1i, 2+0i; 0+1i, 1+0i];"
         "Z = pagemtimes(X, Y);");
    EXPECT_DOUBLE_EQ(evalScalar("isreal(Z);"), 0.0);
    // Z(1,1) = X(1,:)*Y(:,1) = 1*(1+1i) + 2*(0+1i) = 1+3i
    EXPECT_DOUBLE_EQ(evalScalar("real(Z(1,1));"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(Z(1,1));"), 3.0);
}

TEST_P(PagemtimesTest, ComplexBatched3D)
{
    eval("X = complex(reshape(1:12, [2, 3, 2]), reshape(13:24, [2, 3, 2]));"  // 2x3x2
         "Y = complex(reshape(1:12, [3, 2, 2]), reshape(13:24, [3, 2, 2]));"  // 3x2x2
         "Z = pagemtimes(X, Y);"
         "Zref1 = X(:,:,1) * Y(:,:,1);"
         "Zref2 = X(:,:,2) * Y(:,:,2);"
         "diff1 = max(abs(reshape(Z(:,:,1) - Zref1, 1, [])));"
         "diff2 = max(abs(reshape(Z(:,:,2) - Zref2, 1, [])));");
    EXPECT_LT(evalScalar("diff1;"), 1e-10);
    EXPECT_LT(evalScalar("diff2;"), 1e-10);
}

TEST_P(PagemtimesTest, ComplexCTransposeConjugates)
{
    // ctranspose: swap + conjugate. Y' = conj(transpose(Y)).
    // X * Y' should differ from X * Y.' (transpose, no conjugate) when imag(Y) != 0.
    eval("X = [1+0i, 2+0i];"             // 1x2
         "Y = [3+1i, 4+2i];"             // 1x2 → Y' is 2x1
         "Zct = pagemtimes(X, 'none', Y, 'ctranspose');"
         "Zt  = pagemtimes(X, 'none', Y, 'transpose');");
    // Y' (ctranspose) = [3-1i; 4-2i] → X*Y' = 1*(3-1i) + 2*(4-2i) = 11-5i
    // Y.' (transpose) = [3+1i; 4+2i] → X*Y.' = 1*(3+1i) + 2*(4+2i) = 11+5i
    EXPECT_DOUBLE_EQ(evalScalar("real(Zct(1,1));"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(Zct(1,1));"), -5.0);
    EXPECT_DOUBLE_EQ(evalScalar("real(Zt(1,1));"),  11.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(Zt(1,1));"),   5.0);
}

TEST_P(PagemtimesTest, ComplexCTransposeOnRealEqualsTranspose)
{
    // For real input, ctranspose ≡ transpose (no imaginary part to flip).
    eval("X = [1, 2];"                   // 1x2
         "Y = [3, 4];"                   // 1x2 → Y' is 2x1
         "Zct = pagemtimes(X, 'none', Y, 'ctranspose');"
         "Zt  = pagemtimes(X, 'none', Y, 'transpose');"
         "diff = max(abs(reshape(Zct - Zt, 1, [])));");
    EXPECT_LT(evalScalar("diff;"), 1e-12);
}

TEST_P(PagemtimesTest, ComplexCTransposeBatched)
{
    // 3D batch with ctranspose on the second operand.
    eval("X = complex(reshape(1:8, [2, 2, 2]), zeros(2, 2, 2));"
         "Y = complex(reshape(1:8, [2, 2, 2]), reshape(1:8, [2, 2, 2]));"
         "Z = pagemtimes(X, 'none', Y, 'ctranspose');"
         "Zref1 = X(:,:,1) * Y(:,:,1)';"
         "Zref2 = X(:,:,2) * Y(:,:,2)';"
         "diff1 = max(abs(reshape(Z(:,:,1) - Zref1, 1, [])));"
         "diff2 = max(abs(reshape(Z(:,:,2) - Zref2, 1, [])));");
    EXPECT_LT(evalScalar("diff1;"), 1e-10);
    EXPECT_LT(evalScalar("diff2;"), 1e-10);
}

INSTANTIATE_DUAL(PagemtimesTest);

// ============================================================
// ndgrid + kron
// ============================================================

class GridKronTest : public DualEngineTest {};

// ── ndgrid ──────────────────────────────────────────────────────

TEST_P(GridKronTest, NdgridTwoArgsShape)
{
    eval("[X, Y] = ndgrid(1:3, 1:5);");
    auto *X = getVarPtr("X");
    auto *Y = getVarPtr("Y");
    // ndgrid: X has shape [numel(x), numel(y)] = [3, 5].
    EXPECT_EQ(rows(*X), 3u);
    EXPECT_EQ(cols(*X), 5u);
    EXPECT_EQ(rows(*Y), 3u);
    EXPECT_EQ(cols(*Y), 5u);
}

TEST_P(GridKronTest, NdgridXVariesAlongRows)
{
    eval("[X, Y] = ndgrid([10 20 30], [1 2 3 4]);");
    auto *X = getVarPtr("X");
    auto *Y = getVarPtr("Y");
    // X(r, c) = x(r)
    for (size_t r = 0; r < 3; ++r)
        for (size_t c = 0; c < 4; ++c)
            EXPECT_DOUBLE_EQ((*X)(r, c), (r + 1) * 10.0)
                << "X at (" << r << "," << c << ")";
    // Y(r, c) = y(c)
    for (size_t r = 0; r < 3; ++r)
        for (size_t c = 0; c < 4; ++c)
            EXPECT_DOUBLE_EQ((*Y)(r, c), static_cast<double>(c + 1))
                << "Y at (" << r << "," << c << ")";
}

TEST_P(GridKronTest, NdgridDiffersFromMeshgridShape)
{
    // meshgrid: [Xm, Ym] = meshgrid(1:3, 1:2) → 2×3 matrices.
    // ndgrid:   [Xn, Yn] = ndgrid  (1:3, 1:2) → 3×2 matrices.
    eval("[Xm, Ym] = meshgrid(1:3, 1:2);"
         "[Xn, Yn] = ndgrid  (1:3, 1:2);");
    EXPECT_DOUBLE_EQ(evalScalar("size(Xm, 1);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Xm, 2);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Xn, 1);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(Xn, 2);"), 2.0);
}

TEST_P(GridKronTest, NdgridThreeArgsShape)
{
    eval("[X, Y, Z] = ndgrid(1:2, 1:3, 1:4);"
         "sx = size(X);");
    auto *sx = getVarPtr("sx");
    EXPECT_EQ(sx->numel(), 3u);
    EXPECT_DOUBLE_EQ(sx->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(sx->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(sx->doubleData()[2], 4.0);
    // Spot-check Z (varies along page dim).
    auto *Z = getVarPtr("Z");
    EXPECT_DOUBLE_EQ((*Z)(0, 0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*Z)(0, 0, 3), 4.0);
    EXPECT_DOUBLE_EQ((*Z)(1, 2, 2), 3.0);
}

TEST_P(GridKronTest, NdgridFourArgsThrows)
{
    EXPECT_THROW(eval("[A, B, C, D] = ndgrid(1:2, 1:2, 1:2, 1:2);"),
                 std::exception);
}

TEST_P(GridKronTest, NdgridSingleArgThrows)
{
    EXPECT_THROW(eval("X = ndgrid(1:5);"), std::exception);
}

// ── kron ────────────────────────────────────────────────────────

TEST_P(GridKronTest, KronOnesShape)
{
    eval("K = kron(ones(2, 3), ones(4, 5));");
    auto *K = getVarPtr("K");
    EXPECT_EQ(rows(*K), 8u);
    EXPECT_EQ(cols(*K), 15u);
    for (size_t i = 0; i < K->numel(); ++i)
        EXPECT_DOUBLE_EQ(K->doubleData()[i], 1.0);
}

TEST_P(GridKronTest, KronIdentityWithMatrix)
{
    // kron(eye(2), B) places B on the diagonal blocks, zeros off-diagonal.
    eval("B = [10 20; 30 40];"
         "K = kron(eye(2), B);");
    auto *K = getVarPtr("K");
    EXPECT_EQ(rows(*K), 4u);
    EXPECT_EQ(cols(*K), 4u);
    // Block (0,0) = B; block (1,1) = B; block (0,1) = (1,0) = 0.
    EXPECT_DOUBLE_EQ((*K)(0, 0), 10.0); EXPECT_DOUBLE_EQ((*K)(0, 1), 20.0);
    EXPECT_DOUBLE_EQ((*K)(1, 0), 30.0); EXPECT_DOUBLE_EQ((*K)(1, 1), 40.0);
    EXPECT_DOUBLE_EQ((*K)(2, 2), 10.0); EXPECT_DOUBLE_EQ((*K)(2, 3), 20.0);
    EXPECT_DOUBLE_EQ((*K)(3, 2), 30.0); EXPECT_DOUBLE_EQ((*K)(3, 3), 40.0);
    // Off-diagonal blocks zero.
    EXPECT_DOUBLE_EQ((*K)(0, 2), 0.0);
    EXPECT_DOUBLE_EQ((*K)(2, 0), 0.0);
}

TEST_P(GridKronTest, KronVectorVector)
{
    // kron([1 2 3], [10 20]) = [10 20 20 40 30 60].
    eval("K = kron([1 2 3], [10 20]);");
    auto *K = getVarPtr("K");
    EXPECT_EQ(rows(*K), 1u);
    EXPECT_EQ(cols(*K), 6u);
    const double expected[] = {10, 20, 20, 40, 30, 60};
    for (size_t i = 0; i < 6; ++i)
        EXPECT_DOUBLE_EQ(K->doubleData()[i], expected[i]);
}

TEST_P(GridKronTest, KronKnownSmallExample)
{
    // [1 2; 3 4] ⊗ [0 1; 1 0] =
    //   [1*0 1*1 2*0 2*1;
    //    1*1 1*0 2*1 2*0;
    //    3*0 3*1 4*0 4*1;
    //    3*1 3*0 4*1 4*0]
    // = [0 1 0 2;
    //    1 0 2 0;
    //    0 3 0 4;
    //    3 0 4 0]
    eval("K = kron([1 2; 3 4], [0 1; 1 0]);");
    auto *K = getVarPtr("K");
    EXPECT_EQ(rows(*K), 4u);
    EXPECT_EQ(cols(*K), 4u);
    const double expected[4][4] = {
        {0, 1, 0, 2},
        {1, 0, 2, 0},
        {0, 3, 0, 4},
        {3, 0, 4, 0},
    };
    for (size_t r = 0; r < 4; ++r)
        for (size_t c = 0; c < 4; ++c)
            EXPECT_DOUBLE_EQ((*K)(r, c), expected[r][c])
                << "at (" << r << "," << c << ")";
}

TEST_P(GridKronTest, KronEmptyAGivesEmpty)
{
    eval("K = kron(zeros(0, 0), [1 2; 3 4]);");
    auto *K = getVarPtr("K");
    EXPECT_EQ(K->numel(), 0u);
}

TEST_P(GridKronTest, KronComplexThrows)
{
    EXPECT_THROW(eval("K = kron([1 2], [3+4i, 5]);"), std::exception);
}

TEST_P(GridKronTest, Kron3DInputThrows)
{
    eval("A = zeros(2, 2, 2);");
    EXPECT_THROW(eval("K = kron(A, [1 2; 3 4]);"), std::exception);
}

INSTANTIATE_DUAL(GridKronTest);

