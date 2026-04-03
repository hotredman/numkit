// tests/test_matrices.cpp — Matrices, indexing, bounds, colon, end keyword
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
// Colon expressions
// ============================================================

class ColonTest : public DualEngineTest
{};

TEST_P(ColonTest, SimpleRange)
{
    eval("v = 1:5;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 5u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 5.0);
}

TEST_P(ColonTest, SteppedRange)
{
    eval("v = 0:2:10;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 6u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[5], 10.0);
}

TEST_P(ColonTest, NegativeStep)
{
    eval("v = 5:-1:1;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 5u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 1.0);
}

TEST_P(ColonTest, EmptyRange)
{
    eval("v = 5:1;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 0u);
}

TEST_P(ColonTest, ColonIndexing)
{
    eval("A = [1 2 3; 4 5 6]; r = A(:, 2);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(rows(*r), 2u);
    EXPECT_EQ(cols(*r), 1u);
    EXPECT_DOUBLE_EQ((*r)(0, 0), 2.0);
    EXPECT_DOUBLE_EQ((*r)(1, 0), 5.0);
}

INSTANTIATE_DUAL(ColonTest);

// ============================================================
// Chain calls / function handles
// ============================================================

class ChainCallTest : public DualEngineTest
{};

TEST_P(ChainCallTest, FuncHandleCall)
{
    eval("f = @sin; r = f(0);");
    EXPECT_NEAR(getVar("r"), 0.0, 1e-12);
}

TEST_P(ChainCallTest, FuncHandleCallPi)
{
    eval("f = @cos; r = f(pi);");
    EXPECT_NEAR(getVar("r"), -1.0, 1e-12);
}

TEST_P(ChainCallTest, AnonFuncCall)
{
    eval("f = @(x) x^2; r = f(5);");
    EXPECT_DOUBLE_EQ(getVar("r"), 25.0);
}

TEST_P(ChainCallTest, AnonFuncWithClosure)
{
    eval("a = 10; f = @(x) x + a; r = f(5);");
    EXPECT_DOUBLE_EQ(getVar("r"), 15.0);
}

INSTANTIATE_DUAL(ChainCallTest);

// ============================================================
// End keyword in indexing
// ============================================================

class EndKeywordTest : public DualEngineTest
{};

TEST_P(EndKeywordTest, EndInIndex)
{
    eval("v = [10 20 30]; r = v(end);");
    EXPECT_DOUBLE_EQ(getVar("r"), 30.0);
}

TEST_P(EndKeywordTest, EndMinusOne)
{
    eval("v = [10 20 30]; r = v(end-1);");
    EXPECT_DOUBLE_EQ(getVar("r"), 20.0);
}

TEST_P(EndKeywordTest, EndInRange)
{
    eval("v = [10 20 30 40 50]; r = v(2:end);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 4u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 20.0);
}

INSTANTIATE_DUAL(EndKeywordTest);

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
// Cell array multi-dimensional indexing
// ============================================================

class CellIndexTest : public DualEngineTest
{};

TEST_P(CellIndexTest, Cell1DGetSet)
{
    eval("c = {10, 'hello', [1 2 3]};");
    EXPECT_DOUBLE_EQ(evalScalar("c{1};"), 10.0);
}

TEST_P(CellIndexTest, Cell1DAssign)
{
    eval("c = {1, 2, 3}; c{2} = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("c{2};"), 99.0);
}

TEST_P(CellIndexTest, Cell2DGet)
{
    eval("c = cell(2, 2); c{1,1} = 10; c{2,1} = 20; c{1,2} = 30; c{2,2} = 40;");
    EXPECT_DOUBLE_EQ(evalScalar("c{2,2};"), 40.0);
    EXPECT_DOUBLE_EQ(evalScalar("c{1,2};"), 30.0);
}

TEST_P(CellIndexTest, Cell2DSet)
{
    eval("c = cell(2, 3); c{1,3} = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("c{1,3};"), 99.0);
}

TEST_P(CellIndexTest, Cell3DGet)
{
    eval("c = cell(2, 2, 2); c{2,2,2} = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("c{2,2,2};"), 42.0);
}

TEST_P(CellIndexTest, Cell3DSet)
{
    eval("c = cell(2, 2, 2); c{1,2,2} = 77;");
    EXPECT_DOUBLE_EQ(evalScalar("c{1,2,2};"), 77.0);
}

TEST_P(CellIndexTest, CellLinearIndex)
{
    eval("c = {10, 20, 30, 40};");
    EXPECT_DOUBLE_EQ(evalScalar("c{3};"), 30.0);
}

INSTANTIATE_DUAL(CellIndexTest);

// ============================================================
// Colon-all with different types
// ============================================================

class ColonAllTest : public DualEngineTest
{};

TEST_P(ColonAllTest, ColonAllRows)
{
    eval("A = [1 2; 3 4]; r = A(:, 1);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 2u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 3.0);
}

TEST_P(ColonAllTest, ColonAllCols)
{
    eval("A = [1 2 3; 4 5 6]; r = A(2, :);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 3u);
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 4.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[2], 6.0);
}

TEST_P(ColonAllTest, ColonAllLinearize)
{
    eval("A = [1 2; 3 4]; r = A(:);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 4u);
    // Column-major: 1, 3, 2, 4
    EXPECT_DOUBLE_EQ(r->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[2], 2.0);
    EXPECT_DOUBLE_EQ(r->doubleData()[3], 4.0);
}

INSTANTIATE_DUAL(ColonAllTest);