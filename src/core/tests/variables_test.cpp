// tests/test_variables.cpp — Assignment, multi-assign, structs, cells, delete
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace m_test;

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

// CSL: [c{:}] / [c{vec}] expands the selected cell contents (a comma-separated
// list) into the array literal. Both backends (TreeWalker + VM HORZCAT_APPEND_CELL_CSL).
TEST_P(CellTest, CellCommaListConcat)
{
    eval("c = {3, 8, 4};");
    eval("v = [c{:}];");  // c{:} -> 3, 8, 4
    auto *v = getVarPtr("v");
    ASSERT_NE(v, nullptr);
    ASSERT_EQ(v->numel(), 3u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 8.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 4.0);
    eval("r = sum([c{:}]);");
    EXPECT_DOUBLE_EQ(getVar("r"), 15.0);
    eval("w = [0, c{:}, 100];");  // CSL mixed with literals -> 0 3 8 4 100
    EXPECT_EQ(getVarPtr("w")->numel(), 5u);
    EXPECT_DOUBLE_EQ(getVarPtr("w")->doubleData()[4], 100.0);
    eval("u = [c{[1 3]}];");  // vector subscript -> 3, 4
    auto *u = getVarPtr("u");
    ASSERT_EQ(u->numel(), 2u);
    EXPECT_DOUBLE_EQ(u->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(u->doubleData()[1], 4.0);
}

// CSL: f(c{:}) splices the cell's contents into the argument list. Both backends
// (TreeWalker buildArgs + VM CALL_VARARGS). v1: a SOLE c{:} argument.
TEST_P(CellTest, CellCommaListCallArgs)
{
    eval("c = {3, 8};");
    eval("m = max(c{:});");  // max(3, 8) = 8 (a 2-arg builtin via CSL)
    EXPECT_DOUBLE_EQ(getVar("m"), 8.0);
    eval("d = {3, 8, 4};");
    eval("h = horzcat(d{:});");  // horzcat(3, 8, 4) = [3 8 4] (N-arg via CSL)
    auto *h = getVarPtr("h");
    ASSERT_NE(h, nullptr);
    ASSERT_EQ(h->numel(), 3u);
    EXPECT_DOUBLE_EQ(h->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(h->doubleData()[2], 4.0);
    eval("p = plus(c{:});");  // plus(3, 8) = 11
    EXPECT_DOUBLE_EQ(getVar("p"), 11.0);
}

// CSL: mixed f(a, c{:}, b) splices the cell among plain args. On the VM this lowers
// to a {a, c{:}, b} cell (CELL_APPEND_ELEM) + CALL_VARARGS; TreeWalker buildArgs
// expands inline. Single output. Exercises c{:} leading / middle / trailing / doubled.
TEST_P(CellTest, CellCommaListMixedCallArgs)
{
    eval("c = {3, 8, 4};");
    eval("h = horzcat(1, c{:}, 9);");  // c{:} in the middle -> [1 3 8 4 9]
    auto *h = getVarPtr("h");
    ASSERT_NE(h, nullptr);
    ASSERT_EQ(h->numel(), 5u);
    EXPECT_DOUBLE_EQ(h->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(h->doubleData()[1], 3.0);
    EXPECT_DOUBLE_EQ(h->doubleData()[4], 9.0);
    eval("t = horzcat(c{:}, 100);");  // c{:} leading -> [3 8 4 100]
    auto *t = getVarPtr("t");
    ASSERT_EQ(t->numel(), 4u);
    EXPECT_DOUBLE_EQ(t->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(t->doubleData()[3], 100.0);
    eval("l = horzcat(0, c{:});");  // c{:} trailing -> [0 3 8 4]
    auto *l = getVarPtr("l");
    ASSERT_EQ(l->numel(), 4u);
    EXPECT_DOUBLE_EQ(l->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(l->doubleData()[3], 4.0);
    eval("dd = horzcat(c{:}, c{:});");  // two CSL args -> [3 8 4 3 8 4]
    auto *dd = getVarPtr("dd");
    ASSERT_EQ(dd->numel(), 6u);
    EXPECT_DOUBLE_EQ(dd->doubleData()[5], 4.0);
}

// CSL: [a,b,c] = d{:} distributes the cell's contents to multiple LHS targets.
// VM CELL_GET_MULTI (compileColonExpr -> COLON_ALL marker -> resolveIndices over
// the cell numel) + TreeWalker execMultiAssign cell branch (resolveIndex colon).
TEST_P(CellTest, CellCommaListMultiAssign)
{
    eval("d = {3, 8, 4};");
    eval("[a, b, c] = d{:};");  // splice all -> a=3, b=8, c=4
    EXPECT_DOUBLE_EQ(getVar("a"), 3.0);
    EXPECT_DOUBLE_EQ(getVar("b"), 8.0);
    EXPECT_DOUBLE_EQ(getVar("c"), 4.0);
    eval("[x, y] = d{[1 3]};");  // vector subscript -> x=3, y=4
    EXPECT_DOUBLE_EQ(getVar("x"), 3.0);
    EXPECT_DOUBLE_EQ(getVar("y"), 4.0);
}

// CSL: {c{:}} re-wraps the selected cell contents into a new cell literal; mixed
// {0, c{:}, 9} splices in the middle. (TreeWalker; VM cell-literal CSL follows.)
TEST_P(CellTest, CellCommaListInCellLiteral)
{
    eval("c = {3, 8, 4};");
    eval("d = {c{:}};");  // re-wrap the contents -> {3, 8, 4}
    auto *d = getVarPtr("d");
    ASSERT_NE(d, nullptr);
    ASSERT_TRUE(d->isCell());
    ASSERT_EQ(d->numel(), 3u);
    EXPECT_DOUBLE_EQ(d->cellAt(0).toScalar(), 3.0);
    EXPECT_DOUBLE_EQ(d->cellAt(2).toScalar(), 4.0);
    eval("e = {0, c{:}, 9};");  // mixed -> {0, 3, 8, 4, 9}
    auto *e = getVarPtr("e");
    ASSERT_TRUE(e->isCell());
    ASSERT_EQ(e->numel(), 5u);
    EXPECT_DOUBLE_EQ(e->cellAt(0).toScalar(), 0.0);
    EXPECT_DOUBLE_EQ(e->cellAt(4).toScalar(), 9.0);
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

// A `global G` declared inside a FUNCTION must not surface in the base
// workspace when the base itself never declared it. Verified vs MATLAB
// R2025b: after calling a function that does `global G; G=42`, the base's
// exist('G','var') is 0. (Regression guard for the deep global leak:
// the VM used to mirror a function's global into workspaceEnv_.)
TEST_P(GlobalTest, GlobalInFunctionDoesNotLeakToBase)
{
    eval(R"(
        function setit()
            global G;
            G = 42;
        end
    )");
    eval("setit();");
    EXPECT_DOUBLE_EQ(evalScalar("exist('G','var')"), 0.0);
}

// Two functions sharing a global see each other's writes, yet the base — which
// never declared it — still does not (matches MATLAB).
TEST_P(GlobalTest, FunctionsShareGlobalWithoutBaseLeak)
{
    eval(R"(
        function setG(v)
            global SHARED;
            SHARED = v;
        end
        function r = getG()
            global SHARED;
            r = SHARED;
        end
    )");
    eval("setG(77);");
    EXPECT_DOUBLE_EQ(evalScalar("getG()"), 77.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('SHARED','var')"), 0.0);
}

// When the base DOES declare the global, it sees a function's modification
// (the legitimate shared-state path must not regress).
TEST_P(GlobalTest, BaseSeesGlobalModifiedByFunction)
{
    eval(R"(
        function bumpit()
            global gg;
            gg = gg + 1;
        end
    )");
    eval("global gg; gg = 10; bumpit();");
    EXPECT_DOUBLE_EQ(getVar("gg"), 11.0);
}

// A global declared but never assigned is [] (empty matrix) in MATLAB —
// visible to exist and readable as an empty value. Both engines seed the
// empty matrix on declaration (TreeWalker::execGlobalPersistent /
// Engine::updateTopLevelGlobals).
TEST_P(GlobalTest, BareGlobalIsEmptyAndVisible)
{
    eval("global gbare;");
    EXPECT_DOUBLE_EQ(evalScalar("exist('gbare','var')"), 1.0);
    EXPECT_TRUE(evalBool("isempty(gbare)"));
}

INSTANTIATE_DUAL(GlobalTest);
