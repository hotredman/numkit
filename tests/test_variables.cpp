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
