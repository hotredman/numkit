// tests/test_variables.cpp — Assignment, multi-assign, structs, cells, delete
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

// ============================================================
// Assignment
// ============================================================

class AssignTest : public DualEngineTest {};

TEST_P(AssignTest, SimpleAssign)
{
    eval("x = 42;");
    EXPECT_DOUBLE_EQ(getVar("x"), 42.0);
}

TEST_P(AssignTest, ChainedAssign)
{
    eval("x = 2; y = x + 3;");
    EXPECT_DOUBLE_EQ(getVar("y"), 5.0);
}

TEST_P(AssignTest, FieldAssign)
{
    eval("s.name = 'hello';");
    auto *s = getVarPtr("s");
    ASSERT_TRUE(s != nullptr);
    EXPECT_TRUE(s->isStruct());
    EXPECT_EQ(s->field("name").toString(), "hello");
}

TEST_P(AssignTest, NestedFieldAssign)
{
    eval("s.a.b = 42;");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->field("a").field("b").toScalar(), 42.0);
}

TEST_P(AssignTest, IndexedAssign)
{
    eval("A = zeros(3,3); A(2,2) = 99;");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(1, 1), 99.0);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 0.0);
}

TEST_P(AssignTest, EmptyMatrixAssign)
{
    eval("x = [];");
    auto *x = getVarPtr("x");
    ASSERT_TRUE(x != nullptr);
    EXPECT_TRUE(x->isEmpty());
}

INSTANTIATE_DUAL(AssignTest);

// ============================================================
// Multi-assign and tilde
// ============================================================

class MultiAssignTest : public DualEngineTest {};

TEST_P(MultiAssignTest, BasicMultiAssign)
{
    eval("function [a, b] = myfun()\n  a = 10;\n  b = 20;\nend");
    eval("[m, n] = myfun();");
    EXPECT_DOUBLE_EQ(getVar("m"), 10.0);
    EXPECT_DOUBLE_EQ(getVar("n"), 20.0);
}

TEST_P(MultiAssignTest, TildeIgnoresOutput)
{
    eval("function [a, b] = myfun()\n  a = 10;\n  b = 20;\nend");
    eval("[~, n] = myfun();");
    EXPECT_DOUBLE_EQ(getVar("n"), 20.0);
    EXPECT_EQ(getVarPtr("~"), nullptr);
}

INSTANTIATE_DUAL(MultiAssignTest);

// ============================================================
// Cell arrays
// ============================================================

class CellTest : public DualEngineTest {};

TEST_P(CellTest, CellCreate)
{
    eval("c = {1, 'hello', [1 2 3]};");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isCell());
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 1.0);
    EXPECT_EQ(c->cellAt(1).toString(), "hello");
    EXPECT_EQ(c->cellAt(2).numel(), 3u);
}

TEST_P(CellTest, CellIndex)
{
    eval("c = {10, 20, 30}; r = c{2};");
    EXPECT_DOUBLE_EQ(getVar("r"), 20.0);
}

INSTANTIATE_DUAL(CellTest);

// ============================================================
// Struct
// ============================================================

class StructTest : public DualEngineTest {};

TEST_P(StructTest, CreateAndAccess)
{
    eval("s.x = 1; s.y = 2;");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->field("x").toScalar(), 1.0);
    EXPECT_DOUBLE_EQ(s->field("y").toScalar(), 2.0);
}

TEST_P(StructTest, NestedStruct)
{
    eval("s.inner.val = 42;");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->field("inner").field("val").toScalar(), 42.0);
}

INSTANTIATE_DUAL(StructTest);

// ============================================================
// Delete assign
// ============================================================

class DeleteTest : public DualEngineTest {};

TEST_P(DeleteTest, DeleteElements)
{
    eval("v = [1 2 3 4 5]; v(3) = [];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 4u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 4.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 5.0);
}

TEST_P(DeleteTest, DeleteMultiple)
{
    eval("v = [1 2 3 4 5]; v([1 3 5]) = [];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 2u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 4.0);
}

TEST_P(DeleteTest, DeleteComplexElements)
{
    eval("v = [1+2i, 3+4i, 5+6i]; v(2) = [];");
    auto *v = getVarPtr("v");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(v->numel(), 2u);
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].real(), 5.0);
}

TEST_P(DeleteTest, DeleteLogicalElements)
{
    eval("v = [true, false, true, false]; v([2 3]) = [];");
    auto *v = getVarPtr("v");
    EXPECT_TRUE(v->isLogical());
    EXPECT_EQ(v->numel(), 2u);
    EXPECT_EQ(v->logicalData()[0], 1);
    EXPECT_EQ(v->logicalData()[1], 0);
}

TEST_P(DeleteTest, DeleteCellElements)
{
    eval("c = {1, 'hello', [1 2 3]}; c(2) = [];");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isCell());
    EXPECT_EQ(c->numel(), 2u);
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 1.0);
    EXPECT_EQ(c->cellAt(1).numel(), 3u); // [1 2 3]
}

TEST_P(DeleteTest, DeleteRow2D)
{
    eval("A = [1 2 3; 4 5 6; 7 8 9]; A(2, :) = [];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 3u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0), 7.0);
}

TEST_P(DeleteTest, DeleteColumn2D)
{
    eval("A = [1 2 3; 4 5 6]; A(:, 2) = [];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 2u);
    EXPECT_DOUBLE_EQ((*A)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(0, 1), 3.0);
}

TEST_P(DeleteTest, DeleteCharElements)
{
    eval("s = 'hello'; s(2:3) = [];");
    auto *s = getVarPtr("s");
    EXPECT_TRUE(s->isChar());
    EXPECT_EQ(s->toString(), "hlo");
}

TEST_P(DeleteTest, DeletePage3D)
{
    eval(R"(
        A = zeros(2, 2, 3);
        A(:,:,1) = [1 2; 3 4];
        A(:,:,2) = [5 6; 7 8];
        A(:,:,3) = [9 10; 11 12];
        A(:,:,2) = [];
    )");
    auto *A = getVarPtr("A");
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 2u);
    EXPECT_EQ(A->dims().pages(), 2u);
    // Page 1 stays [1 2; 3 4], page 2 becomes old page 3 [9 10; 11 12]
    EXPECT_DOUBLE_EQ((*A)(0, 0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1, 0), 4.0);
    EXPECT_DOUBLE_EQ((*A)(0, 0, 1), 9.0);
    EXPECT_DOUBLE_EQ((*A)(1, 1, 1), 12.0);
}

TEST_P(DeleteTest, DeleteRow3D)
{
    eval(R"(
        A = zeros(3, 2, 2);
        A(:,:,1) = [1 2; 3 4; 5 6];
        A(:,:,2) = [7 8; 9 10; 11 12];
        A(2,:,:) = [];
    )");
    auto *A = getVarPtr("A");
    EXPECT_EQ(A->dims().rows(), 2u);
    EXPECT_EQ(A->dims().cols(), 2u);
    EXPECT_EQ(A->dims().pages(), 2u);
    EXPECT_DOUBLE_EQ((*A)(0, 0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0, 0), 5.0);
    EXPECT_DOUBLE_EQ((*A)(0, 0, 1), 7.0);
    EXPECT_DOUBLE_EQ((*A)(1, 0, 1), 11.0);
}

TEST_P(DeleteTest, DeleteCellRow2D)
{
    eval("c = {1 2 3; 4 5 6}; c(1, :) = [];");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isCell());
    EXPECT_EQ(c->dims().rows(), 1u);
    EXPECT_EQ(c->dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 4.0);
    EXPECT_DOUBLE_EQ(c->cellAt(1).toScalar(), 5.0);
    EXPECT_DOUBLE_EQ(c->cellAt(2).toScalar(), 6.0);
}

TEST_P(DeleteTest, DeleteCellColumn2D)
{
    eval("c = {1 2 3; 4 5 6}; c(:, 2) = [];");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isCell());
    EXPECT_EQ(c->dims().rows(), 2u);
    EXPECT_EQ(c->dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 1.0); // (1,1)
    EXPECT_DOUBLE_EQ(c->cellAt(1).toScalar(), 4.0); // (2,1)
    EXPECT_DOUBLE_EQ(c->cellAt(2).toScalar(), 3.0); // (1,2) — was col 3
    EXPECT_DOUBLE_EQ(c->cellAt(3).toScalar(), 6.0); // (2,2)
}

TEST_P(DeleteTest, DeleteCellPage3D)
{
    eval(R"(
        c = cell(2, 2, 2);
        c{1,1,1} = 1; c{2,1,1} = 2; c{1,2,1} = 3; c{2,2,1} = 4;
        c{1,1,2} = 5; c{2,1,2} = 6; c{1,2,2} = 7; c{2,2,2} = 8;
        c(:,:,1) = [];
    )");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isCell());
    EXPECT_EQ(c->dims().rows(), 2u);
    EXPECT_EQ(c->dims().cols(), 2u);
    EXPECT_EQ(c->dims().pages(), 1u);
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 5.0);
    EXPECT_DOUBLE_EQ(c->cellAt(1).toScalar(), 6.0);
    EXPECT_DOUBLE_EQ(c->cellAt(2).toScalar(), 7.0);
    EXPECT_DOUBLE_EQ(c->cellAt(3).toScalar(), 8.0);
}

TEST_P(DeleteTest, DeleteCellRow3D)
{
    eval(R"(
        c = cell(3, 2, 2);
        c{1,1,1} = 10; c{2,1,1} = 20; c{3,1,1} = 30;
        c{1,2,1} = 40; c{2,2,1} = 50; c{3,2,1} = 60;
        c{1,1,2} = 70; c{2,1,2} = 80; c{3,1,2} = 90;
        c{1,2,2} = 100; c{2,2,2} = 110; c{3,2,2} = 120;
        c(2,:,:) = [];
    )");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isCell());
    EXPECT_EQ(c->dims().rows(), 2u);
    EXPECT_EQ(c->dims().cols(), 2u);
    EXPECT_EQ(c->dims().pages(), 2u);
    // Page 1: [10 40; 30 60], Page 2: [70 100; 90 120]
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 10.0); // (1,1,1)
    EXPECT_DOUBLE_EQ(c->cellAt(1).toScalar(), 30.0); // (2,1,1) — was row 3
    EXPECT_DOUBLE_EQ(c->cellAt(4).toScalar(), 70.0); // (1,1,2)
    EXPECT_DOUBLE_EQ(c->cellAt(5).toScalar(), 90.0); // (2,1,2) — was row 3
}

TEST_P(DeleteTest, DeleteCell1D)
{
    eval("c = {10, 20, 30, 40, 50}; c([2 4]) = [];");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isCell());
    EXPECT_EQ(c->numel(), 3u);
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 10.0);
    EXPECT_DOUBLE_EQ(c->cellAt(1).toScalar(), 30.0);
    EXPECT_DOUBLE_EQ(c->cellAt(2).toScalar(), 50.0);
}

INSTANTIATE_DUAL(DeleteTest);

// ============================================================
// Global variables
// ============================================================

class GlobalTest : public DualEngineTest {};

TEST_P(GlobalTest, GlobalVariable)
{
    eval(R"(
        function setg()
            global g;
            g = 42;
        end
    )");
    eval("global g; setg();");
    EXPECT_DOUBLE_EQ(getVar("g"), 42.0);
}

INSTANTIATE_DUAL(GlobalTest);
