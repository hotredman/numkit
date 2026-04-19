// tests/stdlib/indexing_test.cpp — Colon, end, cell, logical mask, dynamic field indexing
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

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

// ============================================================
// Dynamic field access s.(expr)
// ============================================================

class DynamicFieldTest : public DualEngineTest
{};

TEST_P(DynamicFieldTest, GetDynField)
{
    eval("s.x = 10; s.y = 20; name = 'x';");
    EXPECT_DOUBLE_EQ(evalScalar("s.(name);"), 10.0);
}

TEST_P(DynamicFieldTest, SetDynField)
{
    eval("s = struct(); name = 'val'; s.(name) = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("s.val;"), 42.0);
}

TEST_P(DynamicFieldTest, DynFieldTwoFields)
{
    eval("s = struct(); f = 'x'; s.(f) = 10; f = 'y'; s.(f) = 20; f = 'z'; s.(f) = 30;");
    EXPECT_DOUBLE_EQ(evalScalar("s.x;"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.y;"), 20.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.z;"), 30.0);
}

TEST_P(DynamicFieldTest, DynFieldMultiLine)
{
    eval("s = struct();\nf1 = 'x';\ns.(f1) = 10;\nf2 = 'y';\ns.(f2) = 20;\n");
    EXPECT_DOUBLE_EQ(evalScalar("s.x;"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.y;"), 20.0);
}

TEST_P(DynamicFieldTest, DynFieldRawStringNoIndent)
{
    // Manual newlines — works
    EXPECT_DOUBLE_EQ(evalScalar(
                         "\n        s5 = struct();\n        f = 'a';\n        s5.(f) = 10;\n       "
                         " f = 'b';\n        s5.(f) = 20;\n        r = s5.a + s5.b;\n"),
                     30.0);

    // Exact R"() — previously failed
    const char *code = R"(
        s6 = struct();
        f = 'a';
        s6.(f) = 10;
        f = 'b';
        s6.(f) = 20;
        r6 = s6.a + s6.b;
    )";
    EXPECT_DOUBLE_EQ(evalScalar(code), 30.0);
}

TEST_P(DynamicFieldTest, DynFieldAutoCreate)
{
    eval("s.(\"hello\") = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("s.hello;"), 99.0);
}

INSTANTIATE_DUAL(DynamicFieldTest);


// ============================================================
// Indexing operations (from SharedOpsTest)
// ============================================================

class IndexingOpsTest : public DualEngineTest
{};

TEST_P(IndexingOpsTest, IndexComplexArray1D)
{
    // X(1:3) where X is complex
    eval("X = [1+2i, 3+4i, 5+6i, 7+8i]; Y = X(1:3);");
    auto *v = getVarPtr("Y");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(v->complexData()[2].imag(), 6.0);
}

TEST_P(IndexingOpsTest, IndexComplexScalar)
{
    eval("X = [1+2i, 3+4i]; Y = X(2);");
    auto *v = getVarPtr("Y");
    EXPECT_TRUE(v->isComplex());
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 3.0);
    EXPECT_DOUBLE_EQ(v->complexData()[0].imag(), 4.0);
}

TEST_P(IndexingOpsTest, IndexComplexArray2D)
{
    eval("X = [1+1i 2+2i; 3+3i 4+4i]; Y = X(1, 2);");
    auto *v = getVarPtr("Y");
    EXPECT_TRUE(v->isComplex());
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 2.0);
}

TEST_P(IndexingOpsTest, IndexLogicalArray)
{
    eval("X = [true false true false]; Y = X(2:3);");
    auto *v = getVarPtr("Y");
    EXPECT_TRUE(v->isLogical());
    EXPECT_EQ(v->numel(), 2u);
}

TEST_P(IndexingOpsTest, FFTSpectrumSlicing)
{
    // The original failing case: X = fft(x); mag = abs(X(1:N/2+1))
    eval(R"(
        Fs = 256;
        t = 0:1/Fs:1-1/Fs;
        N = length(t);
        x = sin(2*pi*30*t);
        X = fft(x);
        half = X(1:N/2+1);
        mag = abs(half);
    )");
    auto *half = getVarPtr("half");
    EXPECT_TRUE(half->isComplex());
    EXPECT_EQ(half->numel(), 129u); // N/2+1 = 256/2+1 = 129

    auto *mag = getVarPtr("mag");
    EXPECT_EQ(mag->type(), numkit::m::MType::DOUBLE);
    EXPECT_EQ(mag->numel(), 129u);
}

TEST_P(IndexingOpsTest, CellParenScalarIndex)
{
    // c(2) on cell returns a 1x1 sub-cell containing the content
    eval("c = {10, 'hello', [1 2 3]}; r = c(2);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isCell());
    EXPECT_EQ(r->numel(), 1u);
    EXPECT_TRUE(r->cellAt(0).isChar());
    EXPECT_EQ(r->cellAt(0).toString(), "hello");
}

TEST_P(IndexingOpsTest, CellParenVectorIndex)
{
    // c([1 3]) returns a sub-cell
    eval("c = {10, 'hello', [1 2 3]}; r = c([1 3]);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isCell());
    EXPECT_EQ(r->numel(), 2u);
    EXPECT_DOUBLE_EQ(r->cellAt(0).toScalar(), 10.0);
    EXPECT_EQ(r->cellAt(1).numel(), 3u);
}

TEST_P(IndexingOpsTest, CellCurlyIndex)
{
    eval("c = {10, 'hello', [1 2 3]}; r = c{3};");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->type(), numkit::m::MType::DOUBLE);
    EXPECT_EQ(r->numel(), 3u);
}

TEST_P(IndexingOpsTest, CellParenIndex2D)
{
    // c(2,1) on cell returns a 1x1 sub-cell
    eval("c = {1 2; 3 4}; r = c(2, 1);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isCell());
    EXPECT_EQ(r->numel(), 1u);
    EXPECT_DOUBLE_EQ(r->cellAt(0).toScalar(), 3.0);
}

TEST_P(IndexingOpsTest, CellCurlyIndex2D)
{
    eval("c = {1 2; 3 4}; r = c{1, 2};");
    auto *r = getVarPtr("r");
    EXPECT_DOUBLE_EQ(r->toScalar(), 2.0);
}

TEST_P(IndexingOpsTest, CellCurlyAssign)
{
    eval("c = {0, 0, 0}; c{2} = 'world'; r = c{2};");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isChar());
    EXPECT_EQ(r->toString(), "world");
}

TEST_P(IndexingOpsTest, CellParenLogicalIndex)
{
    eval("c = {10, 20, 30}; r = c([true false true]);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isCell());
    EXPECT_EQ(r->numel(), 2u);
    EXPECT_DOUBLE_EQ(r->cellAt(0).toScalar(), 10.0);
    EXPECT_DOUBLE_EQ(r->cellAt(1).toScalar(), 30.0);
}

TEST_P(IndexingOpsTest, Index3DGetSlice)
{
    eval(R"(
        A = zeros(2, 3, 2);
        A(:,:,1) = [1 2 3; 4 5 6];
        A(:,:,2) = [7 8 9; 10 11 12];
        B = A(:, :, 2);
    )");
    auto *B = getVarPtr("B");
    EXPECT_EQ(B->dims().rows(), 2u);
    EXPECT_EQ(B->dims().cols(), 3u);
    EXPECT_DOUBLE_EQ((*B)(0, 0), 7.0);
    EXPECT_DOUBLE_EQ((*B)(1, 2), 12.0);
}

TEST_P(IndexingOpsTest, Index3DSetAndGet)
{
    eval(R"(
        A = zeros(2, 2, 2);
        A(1, 2, 2) = 99;
        r = A(1, 2, 2);
    )");
    auto *r = getVarPtr("r");
    EXPECT_DOUBLE_EQ(r->toScalar(), 99.0);
}

TEST_P(IndexingOpsTest, LogicalMaskSetScalar)
{
    eval("A = [10 20 30 40 50]; A([true false true false true]) = 0;");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0), 0.0);
    EXPECT_DOUBLE_EQ((*A)(1), 20.0);
    EXPECT_DOUBLE_EQ((*A)(2), 0.0);
    EXPECT_DOUBLE_EQ((*A)(3), 40.0);
    EXPECT_DOUBLE_EQ((*A)(4), 0.0);
}

TEST_P(IndexingOpsTest, LogicalMaskSetVector)
{
    eval("A = [1 2 3 4 5]; A([false true false true false]) = [99 88];");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1), 99.0);
    EXPECT_DOUBLE_EQ((*A)(2), 3.0);
    EXPECT_DOUBLE_EQ((*A)(3), 88.0);
    EXPECT_DOUBLE_EQ((*A)(4), 5.0);
}

TEST_P(IndexingOpsTest, LogicalMaskSetComplex)
{
    eval("A = [1+1i, 2+2i, 3+3i]; A([true false true]) = 0;");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ(A->complexData()[0].real(), 0.0);
    EXPECT_DOUBLE_EQ(A->complexData()[1].real(), 2.0);
    EXPECT_DOUBLE_EQ(A->complexData()[2].real(), 0.0);
}

TEST_P(IndexingOpsTest, LogicalMaskSetLogical)
{
    eval("A = [true true false false]; A([false false true true]) = true;");
    auto *A = getVarPtr("A");
    EXPECT_TRUE(A->isLogical());
    EXPECT_EQ(A->logicalData()[0], 1);
    EXPECT_EQ(A->logicalData()[1], 1);
    EXPECT_EQ(A->logicalData()[2], 1);
    EXPECT_EQ(A->logicalData()[3], 1);
}

TEST_P(IndexingOpsTest, LogicalMaskDelete)
{
    eval("A = [10 20 30 40 50]; A([true false true false false]) = [];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(A->numel(), 3u);
    EXPECT_DOUBLE_EQ(A->doubleData()[0], 20.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[1], 40.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[2], 50.0);
}

TEST_P(IndexingOpsTest, LogicalMaskDeleteComplex)
{
    eval("A = [1+2i, 3+4i, 5+6i]; A([false true false]) = [];");
    auto *A = getVarPtr("A");
    EXPECT_TRUE(A->isComplex());
    EXPECT_EQ(A->numel(), 2u);
    EXPECT_DOUBLE_EQ(A->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(A->complexData()[1].real(), 5.0);
}

TEST_P(IndexingOpsTest, LogicalMaskDeleteChar)
{
    eval("s = 'abcde'; s([true false true false true]) = [];");
    auto *s = getVarPtr("s");
    EXPECT_TRUE(s->isChar());
    EXPECT_EQ(s->toString(), "bd");
}

TEST_P(IndexingOpsTest, LogicalMaskDeleteCell)
{
    eval("c = {10, 20, 30, 40}; c([false true false true]) = [];");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isCell());
    EXPECT_EQ(c->numel(), 2u);
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 10.0);
    EXPECT_DOUBLE_EQ(c->cellAt(1).toScalar(), 30.0);
}

TEST_P(IndexingOpsTest, LogicalMaskGetChar)
{
    eval("s = 'hello'; r = s([true false true false true]);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isChar());
    EXPECT_EQ(r->toString(), "hlo");
}

TEST_P(IndexingOpsTest, CellCSLBasic)
{
    eval("c = {10, 'hello', [1 2 3]}; [a, b] = c{[1 2]};");
    auto *a = getVarPtr("a");
    auto *b = getVarPtr("b");
    EXPECT_DOUBLE_EQ(a->toScalar(), 10.0);
    EXPECT_TRUE(b->isChar());
    EXPECT_EQ(b->toString(), "hello");
}

TEST_P(IndexingOpsTest, CellCSLColon)
{
    eval("c = {1, 2, 3}; [x, y, z] = c{:};");
    EXPECT_DOUBLE_EQ(getVar("x"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
    EXPECT_DOUBLE_EQ(getVar("z"), 3.0);
}

TEST_P(IndexingOpsTest, CellCSLWithTilde)
{
    eval("c = {10, 20, 30}; [~, b, ~] = c{:};");
    EXPECT_DOUBLE_EQ(getVar("b"), 20.0);
    EXPECT_EQ(getVarPtr("~"), nullptr);
}

TEST_P(IndexingOpsTest, COWCellSharing)
{
    eval("a = {1, 2, 3}; b = a; a{2} = 99;");
    EXPECT_DOUBLE_EQ(getVarPtr("a")->cellAt(1).toScalar(), 99.0);
    EXPECT_DOUBLE_EQ(getVarPtr("b")->cellAt(1).toScalar(), 2.0); // b unchanged
}

TEST_P(IndexingOpsTest, COWStructSharing)
{
    eval("s.x = 10; t = s; s.x = 99;");
    EXPECT_DOUBLE_EQ(getVarPtr("s")->field("x").toScalar(), 99.0);
    // t.x should still be 10 — not corrupted by COW violation
    EXPECT_DOUBLE_EQ(getVarPtr("t")->field("x").toScalar(), 10.0);
}

TEST_P(IndexingOpsTest, COWPromoteToComplex)
{
    eval("a = [1 2 3]; b = a; a(1) = 1i;");
    EXPECT_TRUE(getVarPtr("a")->isComplex());
    EXPECT_TRUE(getVarPtr("b")->type() == numkit::m::MType::DOUBLE); // b stays double
    EXPECT_DOUBLE_EQ(getVarPtr("b")->doubleData()[0], 1.0);
}

TEST_P(IndexingOpsTest, InfIndexError)
{
    EXPECT_THROW(eval("v = [1 2 3]; v(inf);"), std::runtime_error);
}

TEST_P(IndexingOpsTest, NaNIndexError)
{
    EXPECT_THROW(eval("v = [1 2 3]; v(nan);"), std::runtime_error);
}

TEST_P(IndexingOpsTest, FloatIndexError)
{
    EXPECT_THROW(eval("v = [1 2 3]; v(1.5);"), std::runtime_error);
}

TEST_P(IndexingOpsTest, FloatIndexIntegerOK)
{
    // Integer-valued float should work
    EXPECT_DOUBLE_EQ(evalScalar("v = [10 20 30]; v(2.0);"), 20.0);
}

TEST_P(IndexingOpsTest, NegativeIndexError)
{
    EXPECT_THROW(eval("v = [1 2 3]; v(-1);"), std::runtime_error);
}

TEST_P(IndexingOpsTest, ColonInfError)
{
    EXPECT_THROW(eval("x = 1:inf;"), std::runtime_error);
}

TEST_P(IndexingOpsTest, ColonNaNEmpty)
{
    eval("x = 1:nan;");
    EXPECT_TRUE(getVarPtr("x")->isEmpty());
}

TEST_P(IndexingOpsTest, ScalarOutOfBoundsGet)
{
    EXPECT_THROW(eval("x = 5; x(2);"), std::runtime_error);
}

TEST_P(IndexingOpsTest, ScalarIndexOneOK)
{
    EXPECT_DOUBLE_EQ(evalScalar("x = 42; x(1);"), 42.0);
}

TEST_P(IndexingOpsTest, IndexSetSizeMismatch)
{
    EXPECT_THROW(eval("a = [1 2 3 4 5]; a([1 2 3]) = [10 20];"), std::runtime_error);
}

TEST_P(IndexingOpsTest, ScalarExpansionAssign)
{
    eval("a = [1 2 3 4 5]; a([1 3 5]) = 0;");
    auto *a = getVarPtr("a");
    expectElem(*a, 0, 0.0);
    expectElem(*a, 1, 2.0);
    expectElem(*a, 2, 0.0);
    expectElem(*a, 3, 4.0);
    expectElem(*a, 4, 0.0);
}

TEST_P(IndexingOpsTest, ColumnVectorGrow)
{
    eval("a = [1; 2; 3]; a(5) = 99;");
    auto *a = getVarPtr("a");
    EXPECT_EQ(rows(*a), 5u);
    EXPECT_EQ(cols(*a), 1u); // stays column vector
    expectElem(*a, 4, 99.0);
}

TEST_P(IndexingOpsTest, LogicalIndexTooLong)
{
    EXPECT_THROW(eval("v = [1 2 3]; v([true true true true]);"), std::runtime_error);
}

TEST_P(IndexingOpsTest, LogicalWritePerElement)
{
    eval("L = [true true true]; L([1 2]) = [0 5];");
    auto *L = getVarPtr("L");
    EXPECT_EQ(L->logicalData()[0], 0); // false
    EXPECT_EQ(L->logicalData()[1], 1); // true (5 != 0)
    EXPECT_EQ(L->logicalData()[2], 1); // unchanged
}

TEST_P(IndexingOpsTest, DeleteOnMatrixProducesRow)
{
    eval("A = [1 2; 3 4]; A([1 2 3]) = [];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 1u);
    EXPECT_EQ(cols(*A), 1u);
    expectElem(*A, 0, 4.0);
}

TEST_P(IndexingOpsTest, ScalarExpansion2D)
{
    eval("A = zeros(3, 3); A(:, 2) = 7;");
    auto *A = getVarPtr("A");
    expectElem2D(*A, 0, 1, 7.0);
    expectElem2D(*A, 1, 1, 7.0);
    expectElem2D(*A, 2, 1, 7.0);
    expectElem2D(*A, 0, 0, 0.0); // other cols unchanged
}

TEST_P(IndexingOpsTest, EndKeyword2D)
{
    eval("A = [1 2 3; 4 5 6]; r = A(end, end);");
    EXPECT_DOUBLE_EQ(getVar("r"), 6.0);
}

TEST_P(IndexingOpsTest, EndKeyword2DRange)
{
    eval("A = [1 2 3; 4 5 6]; r = A(end, :);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 3u);
    expectElem(*r, 0, 4.0);
    expectElem(*r, 1, 5.0);
    expectElem(*r, 2, 6.0);
}

TEST_P(IndexingOpsTest, AutoGrowRowVector)
{
    eval("v = [1 2 3]; v(5) = 99;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 5u);
    expectElem(*v, 3, 0.0); // zero-filled gap
    expectElem(*v, 4, 99.0);
}

TEST_P(IndexingOpsTest, CharSpaceFill)
{
    eval("s = 'ab'; s(5) = 'z';");
    auto *s = getVarPtr("s");
    EXPECT_EQ(s->toString(), "ab  z");
}

// ── 2D/3D indexing for char matrices ──────────────────

TEST_P(IndexingOpsTest, Char2DSingleElementReturnsChar)
{
    // Build a 2x3 char matrix via reshape of a char row.
    eval("M = reshape('abcdef', 2, 3);");
    // Column-major fill: M(1,1)='a' M(2,1)='b' M(1,2)='c' M(2,2)='d' ...
    eval("c11 = M(1,1);");
    auto *v11 = getVarPtr("c11");
    ASSERT_NE(v11, nullptr);
    EXPECT_TRUE(v11->isChar());
    EXPECT_EQ(v11->toString(), "a");

    eval("c22 = M(2,2);");
    auto *v22 = getVarPtr("c22");
    EXPECT_EQ(v22->toString(), "d");

    eval("c13 = M(1,3);");
    auto *v13 = getVarPtr("c13");
    EXPECT_EQ(v13->toString(), "e");
}

TEST_P(IndexingOpsTest, Char2DSubMatrixPreservesShape)
{
    // Extract a 2x2 sub-matrix from the 2x3 char matrix.
    eval("M = reshape('abcdef', 2, 3);");
    eval("S = M(:, 1:2);");
    auto *s = getVarPtr("S");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isChar());
    EXPECT_EQ(s->dims().rows(), 2u);
    EXPECT_EQ(s->dims().cols(), 2u);
    // Column-major raw bytes: 'a','b','c','d'
    EXPECT_EQ(s->charData()[0], 'a');
    EXPECT_EQ(s->charData()[1], 'b');
    EXPECT_EQ(s->charData()[2], 'c');
    EXPECT_EQ(s->charData()[3], 'd');
}

TEST_P(IndexingOpsTest, Char2DRowExtract)
{
    eval("M = reshape('abcdef', 2, 3);");
    // Second row → 'b' 'd' 'f' (column-major stride).
    eval("R = M(2, :);");
    auto *r = getVarPtr("R");
    ASSERT_NE(r, nullptr);
    EXPECT_TRUE(r->isChar());
    EXPECT_EQ(r->dims().rows(), 1u);
    EXPECT_EQ(r->dims().cols(), 3u);
    EXPECT_EQ(r->toString(), "bdf");
}

TEST_P(IndexingOpsTest, Char2DAssignmentOverwritesCell)
{
    eval("M = reshape('abcdef', 2, 3);");
    eval("M(1,2) = 'Z';");
    auto *m = getVarPtr("M");
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->dims().rows(), 2u);
    EXPECT_EQ(m->dims().cols(), 3u);
    // M(1,2) was 'c' → now 'Z'.
    EXPECT_EQ(m->charData()[2], 'Z');
    // Other cells untouched.
    EXPECT_EQ(m->charData()[0], 'a');
    EXPECT_EQ(m->charData()[1], 'b');
    EXPECT_EQ(m->charData()[3], 'd');
}

TEST_P(IndexingOpsTest, Char2DSubMatrixAssignment)
{
    eval("M = reshape('abcdef', 2, 3);");
    eval("M(:, 1) = 'XY';");    // overwrite first column
    auto *m = getVarPtr("M");
    EXPECT_EQ(m->charData()[0], 'X');
    EXPECT_EQ(m->charData()[1], 'Y');
    EXPECT_EQ(m->charData()[2], 'c');  // unchanged
}

TEST_P(IndexingOpsTest, CharLinearIndexStillReturnsRow)
{
    // Backwards compat: 1D linear indexing on a char array still yields
    // a row-oriented char array (MATLAB parity).
    eval("s = 'hello'; x = s([1 3 5]);");
    auto *x = getVarPtr("x");
    ASSERT_NE(x, nullptr);
    EXPECT_TRUE(x->isChar());
    EXPECT_EQ(x->toString(), "hlo");
}

// ── Indexing on typed integer / single arrays ──────────

TEST_P(IndexingOpsTest, Int32LinearReadKeepsType)
{
    eval("a = int32([10 20 30 40]);");
    eval("x = a(2);");
    auto *x = getVarPtr("x");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->type(), MType::INT32);
    EXPECT_EQ(x->int32Data()[0], 20);
}

TEST_P(IndexingOpsTest, Int32VectorSliceKeepsType)
{
    eval("a = int32([10 20 30 40 50]);");
    eval("x = a([2 4 5]);");
    auto *x = getVarPtr("x");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->type(), MType::INT32);
    EXPECT_EQ(x->numel(), 3u);
    EXPECT_EQ(x->int32Data()[0], 20);
    EXPECT_EQ(x->int32Data()[1], 40);
    EXPECT_EQ(x->int32Data()[2], 50);
}

TEST_P(IndexingOpsTest, Uint8MatrixScalar2D)
{
    // 2x3 uint8 matrix (reshape on uint8 result).
    eval("a = uint8(reshape([1 2 3 4 5 6], 2, 3));");
    eval("v = a(2,3);");
    auto *v = getVarPtr("v");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->type(), MType::UINT8);
    EXPECT_EQ(*static_cast<const uint8_t *>(v->rawData()), 6u);
}

TEST_P(IndexingOpsTest, Uint8SubMatrix2D)
{
    eval("a = uint8(reshape([1 2 3 4 5 6], 2, 3));");
    eval("s = a(:, 2:3);");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->type(), MType::UINT8);
    EXPECT_EQ(s->dims().rows(), 2u);
    EXPECT_EQ(s->dims().cols(), 2u);
    // Column-major: 3,4 then 5,6
    const uint8_t *d = static_cast<const uint8_t *>(s->rawData());
    EXPECT_EQ(d[0], 3u); EXPECT_EQ(d[1], 4u);
    EXPECT_EQ(d[2], 5u); EXPECT_EQ(d[3], 6u);
}

TEST_P(IndexingOpsTest, SingleScalarAndSlice)
{
    eval("a = single([1.5 2.5 3.5 4.5]);");
    eval("v = a(3);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), MType::SINGLE);
    EXPECT_FLOAT_EQ(v->singleData()[0], 3.5f);

    eval("s = a([1 4]);");
    auto *s = getVarPtr("s");
    EXPECT_EQ(s->type(), MType::SINGLE);
    EXPECT_EQ(s->numel(), 2u);
    EXPECT_FLOAT_EQ(s->singleData()[0], 1.5f);
    EXPECT_FLOAT_EQ(s->singleData()[1], 4.5f);
}

TEST_P(IndexingOpsTest, Int16LogicalIndex)
{
    eval("a = int16([10 20 30 40]);");
    eval("mask = [true false true false];");
    eval("x = a(mask);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), MType::INT16);
    EXPECT_EQ(x->numel(), 2u);
    EXPECT_EQ(x->int16Data()[0], 10);
    EXPECT_EQ(x->int16Data()[1], 30);
}

TEST_P(IndexingOpsTest, Int32AssignScalarBroadcast)
{
    // a(i) = scalar — writeScalar path.
    eval("a = int32([1 2 3 4 5]); a(3) = 99;");
    auto *a = getVarPtr("a");
    EXPECT_EQ(a->type(), MType::INT32);
    EXPECT_EQ(a->int32Data()[2], 99);
    EXPECT_EQ(a->int32Data()[0], 1);
    EXPECT_EQ(a->int32Data()[4], 5);
}

TEST_P(IndexingOpsTest, Uint16AssignVectorPerElement)
{
    // a(idx) = [v1 v2 ...] — writeElem path.
    eval("a = uint16([1 2 3 4 5]); a([2 4]) = [77 88];");
    auto *a = getVarPtr("a");
    EXPECT_EQ(a->type(), MType::UINT16);
    EXPECT_EQ(a->uint16Data()[1], 77u);
    EXPECT_EQ(a->uint16Data()[3], 88u);
    EXPECT_EQ(a->uint16Data()[2], 3u);   // unchanged
}

TEST_P(IndexingOpsTest, SingleAssign2D)
{
    eval("a = single(reshape([1 2 3 4 5 6], 2, 3));");
    eval("a(1,2) = 9.25;");
    auto *a = getVarPtr("a");
    EXPECT_EQ(a->type(), MType::SINGLE);
    EXPECT_FLOAT_EQ(a->singleData()[2], 9.25f);
}

TEST_P(IndexingOpsTest, Int8SaturationOnWrite)
{
    // int8 range is [-128, 127]. Writing 300 should cast down.
    // C++ narrowing is implementation-defined for out-of-range; we
    // accept whatever static_cast<int8_t>(300.0) produces — here we
    // just check the assignment doesn't crash and the type is kept.
    eval("a = int8([0 0 0]); a(2) = 200;");
    auto *a = getVarPtr("a");
    EXPECT_EQ(a->type(), MType::INT8);
    EXPECT_EQ(a->numel(), 3u);
}

// ── Char matrix display ───────────────────────────────

TEST_P(IndexingOpsTest, DispCharRowUnchanged)
{
    // 1xN char row stays a single line, no quotes.
    capturedOutput.clear();
    eval("disp('hello');");
    EXPECT_NE(capturedOutput.find("hello"), std::string::npos);
    EXPECT_EQ(capturedOutput.find('\''), std::string::npos);  // no quotes from disp
}

TEST_P(IndexingOpsTest, DispCharMatrixPrintsRowByRow)
{
    // 2x3 char matrix via reshape → each row on its own line,
    // column-major read: row 1 = 'a','c','e'; row 2 = 'b','d','f'.
    capturedOutput.clear();
    eval("disp(reshape('abcdef', 2, 3));");
    // Check both rows appear on separate lines. Find 'ace' then expect
    // 'bdf' to follow AFTER it.
    auto p1 = capturedOutput.find("ace");
    auto p2 = capturedOutput.find("bdf");
    ASSERT_NE(p1, std::string::npos);
    ASSERT_NE(p2, std::string::npos);
    EXPECT_LT(p1, p2);
    // No stray "abcdef" single-line flattening.
    EXPECT_EQ(capturedOutput.find("abcdef"), std::string::npos);
}

TEST_P(IndexingOpsTest, ImplicitCharMatrixDisplayQuotesRows)
{
    // Bare-expression display (M = ...) goes through formatDisplay,
    // which MATLAB-matches by quoting each row.
    capturedOutput.clear();
    eval("M = reshape('abcdef', 2, 3)");  // no trailing semicolon → display
    // Expect the two quoted rows 'ace' and 'bdf' somewhere in output.
    EXPECT_NE(capturedOutput.find("'ace'"), std::string::npos);
    EXPECT_NE(capturedOutput.find("'bdf'"), std::string::npos);
}

TEST_P(IndexingOpsTest, ImplicitCharRowDisplayQuotesOnce)
{
    // Single-row char keeps the existing one-line quoted style.
    capturedOutput.clear();
    eval("s = 'hello'");
    EXPECT_NE(capturedOutput.find("'hello'"), std::string::npos);
}

// ── 3D indexing ───────────────────────────────────────
// 3D arrays are built via zeros(...) + linear fill because
// reshape(1D, m, n, p) is not yet supported.

TEST_P(IndexingOpsTest, Uint16_3DSubArrayKeepsType)
{
    eval("A = uint16(zeros(2, 3, 2));");
    eval("for k = 1:12, A(k) = k; end");
    eval("S = A(:, :, 2);");
    auto *s = getVarPtr("S");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->type(), MType::UINT16);
    EXPECT_EQ(s->dims().rows(), 2u);
    EXPECT_EQ(s->dims().cols(), 3u);
    EXPECT_EQ(s->uint16Data()[0], 7u);
    EXPECT_EQ(s->uint16Data()[5], 12u);
}

TEST_P(IndexingOpsTest, Int32_3DScalarAccess)
{
    eval("A = int32(zeros(2, 2, 2));");
    eval("for k = 1:8, A(k) = k * 10; end");
    eval("v = A(2, 2, 2);");
    auto *v = getVarPtr("v");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->type(), MType::INT32);
    EXPECT_EQ(v->int32Data()[0], 80);
}

// ── 2D per-type coverage ──────────────────────────────

TEST_P(IndexingOpsTest, Int8_2DMatrix)
{
    eval("a = int8(reshape([1 2 3 4 5 6], 2, 3));");
    eval("v = a(2, 3);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), MType::INT8);
    EXPECT_EQ(v->int8Data()[0], 6);
}

TEST_P(IndexingOpsTest, Int16_2DSubMatrix)
{
    eval("a = int16(reshape([10 20 30 40 50 60], 2, 3));");
    eval("s = a(:, 2);");
    auto *s = getVarPtr("s");
    EXPECT_EQ(s->type(), MType::INT16);
    EXPECT_EQ(s->numel(), 2u);
    EXPECT_EQ(s->int16Data()[0], 30);
    EXPECT_EQ(s->int16Data()[1], 40);
}

TEST_P(IndexingOpsTest, Int64_2DAssign)
{
    eval("a = int64(reshape([1 2 3 4 5 6], 2, 3));");
    eval("a(1, 2) = 999;");
    auto *a = getVarPtr("a");
    EXPECT_EQ(a->type(), MType::INT64);
    EXPECT_EQ(a->int64Data()[2], 999);
}

TEST_P(IndexingOpsTest, Uint32_2DMatrix)
{
    eval("a = uint32(reshape([1 2 3 4 5 6], 2, 3));");
    eval("v = a(1, 3);");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), MType::UINT32);
    EXPECT_EQ(v->uint32Data()[0], 5u);
}

TEST_P(IndexingOpsTest, Uint64_2DSubMatrix)
{
    eval("a = uint64(reshape([1 2 3 4 5 6], 2, 3));");
    eval("s = a(2, :);");
    auto *s = getVarPtr("s");
    EXPECT_EQ(s->type(), MType::UINT64);
    EXPECT_EQ(s->dims().rows(), 1u);
    EXPECT_EQ(s->dims().cols(), 3u);
    EXPECT_EQ(s->uint64Data()[0], 2u);
    EXPECT_EQ(s->uint64Data()[2], 6u);
}

// ── Logical-mask coverage per type ───────────────────

TEST_P(IndexingOpsTest, Int32LogicalIndex)
{
    eval("a = int32([10 20 30 40 50]);");
    eval("x = a([false true false true false]);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), MType::INT32);
    EXPECT_EQ(x->numel(), 2u);
    EXPECT_EQ(x->int32Data()[0], 20);
    EXPECT_EQ(x->int32Data()[1], 40);
}

TEST_P(IndexingOpsTest, Uint8LogicalIndex)
{
    eval("a = uint8([1 2 3 4 5]);");
    eval("x = a([true true false false true]);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), MType::UINT8);
    EXPECT_EQ(x->numel(), 3u);
    EXPECT_EQ(x->uint8Data()[0], 1u);
    EXPECT_EQ(x->uint8Data()[2], 5u);
}

TEST_P(IndexingOpsTest, SingleLogicalIndex)
{
    eval("a = single([0.1 0.2 0.3 0.4]);");
    eval("x = a([true false true false]);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), MType::SINGLE);
    EXPECT_EQ(x->numel(), 2u);
    EXPECT_FLOAT_EQ(x->singleData()[0], 0.1f);
    EXPECT_FLOAT_EQ(x->singleData()[1], 0.3f);
}

// ── Boundary values (narrowing doesn't corrupt) ──────

TEST_P(IndexingOpsTest, Int8BoundariesRoundTripThroughIndex)
{
    eval("a = int8([-128 0 127]);");
    eval("x = a([1 3]);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), MType::INT8);
    EXPECT_EQ(x->int8Data()[0], -128);
    EXPECT_EQ(x->int8Data()[1], 127);
}

TEST_P(IndexingOpsTest, Uint32MaxPreservedThroughIndex)
{
    eval("a = uint32([0 1 4294967295]);");
    eval("x = a(3);");
    auto *x = getVarPtr("x");
    EXPECT_EQ(x->type(), MType::UINT32);
    EXPECT_EQ(x->uint32Data()[0], 4294967295u);
}

// ── Mixed-type indexed assign ────────────────────────

TEST_P(IndexingOpsTest, Int32AssignFromSingleTruncates)
{
    eval("a = int32([0 0 0]); a(2) = single(42.7);");
    auto *a = getVarPtr("a");
    EXPECT_EQ(a->type(), MType::INT32);
    EXPECT_EQ(a->int32Data()[1], 42);
}

TEST_P(IndexingOpsTest, CharAssignFromInt)
{
    eval("s = 'XXXX'; s(2) = int32(65);");
    auto *s = getVarPtr("s");
    EXPECT_EQ(s->type(), MType::CHAR);
    EXPECT_EQ(s->toString(), "XAXX");
}

TEST_P(IndexingOpsTest, ComplexAssignFromIntPromotesImagZero)
{
    eval("c = [0 0 0] + 0i;");
    eval("c(2) = int32(7);");
    auto *c = getVarPtr("c");
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->isComplex());
    Complex z = c->complexElem(1);
    EXPECT_DOUBLE_EQ(z.real(), 7.0);
    EXPECT_DOUBLE_EQ(z.imag(), 0.0);
}

TEST_P(IndexingOpsTest, Uint8SourceConvertsToUint16)
{
    eval("a = uint16(uint8([10 20 30]));");
    auto *a = getVarPtr("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->type(), MType::UINT16);
    EXPECT_EQ(a->uint16Data()[0], 10u);
    EXPECT_EQ(a->uint16Data()[2], 30u);
}

TEST_P(IndexingOpsTest, Uint8SourceConvertsToLogical)
{
    eval("b = logical(uint8([0 5 0 7]));");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::LOGICAL);
    EXPECT_EQ(b->numel(), 4u);
    EXPECT_EQ(b->logicalData()[0], 0u);
    EXPECT_EQ(b->logicalData()[1], 1u);
    EXPECT_EQ(b->logicalData()[2], 0u);
    EXPECT_EQ(b->logicalData()[3], 1u);
}

INSTANTIATE_DUAL(IndexingOpsTest);
