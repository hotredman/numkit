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
// Workspace introspection inside functions (clear/exist/who/whos)
// ============================================================

class WorkspaceScopeTest : public DualEngineTest
{};

// ── clear ──

TEST_P(WorkspaceScopeTest, ClearLocalDoesNotAffectGlobal)
{
    eval("x = 100;");
    eval(R"(
        function r = test_clear()
            x = 999;
            clear x;
            r = 1;
        end
    )");
    eval("test_clear();");
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 100.0);
}

TEST_P(WorkspaceScopeTest, ClearLocalVariable)
{
    eval(R"(
        function r = test_clear2()
            x = 42;
            clear x;
            r = exist('x');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_clear2();"), 0.0);
}

TEST_P(WorkspaceScopeTest, ClearFunctionCallStyle)
{
    eval(R"(
        function r = test_clear3()
            x = 42;
            clear('x');
            r = exist('x');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_clear3();"), 0.0);
}

// ── exist ──

TEST_P(WorkspaceScopeTest, ExistLocalVar)
{
    eval(R"(
        function r = test_exist()
            x = 42;
            r = exist('x');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_exist();"), 1.0);
}

TEST_P(WorkspaceScopeTest, ExistMissingVar)
{
    eval(R"(
        function r = test_exist2()
            r = exist('zzz_nonexistent');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_exist2();"), 0.0);
}

TEST_P(WorkspaceScopeTest, ExistFunction)
{
    eval(R"(
        function r = test_exist3()
            r = exist('sin');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_exist3();"), 5.0);
}

TEST_P(WorkspaceScopeTest, ExistRuntimeString)
{
    eval(R"(
        function r = test_exist4()
            x = 42;
            name = 'x';
            r = exist(name);
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_exist4();"), 1.0);
}

TEST_P(WorkspaceScopeTest, ExistAfterClear)
{
    eval(R"(
        function r = test_exist5()
            x = 42;
            clear x;
            r = exist('x');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_exist5();"), 0.0);
}

// ── who ──

TEST_P(WorkspaceScopeTest, WhoInsideFunction)
{
    eval(R"(
        function r = test_who()
            a = 1;
            b = 2;
            who;
            r = 1;
        end
    )");
    eval("test_who();");
    EXPECT_NE(capturedOutput.find("a"), std::string::npos);
    EXPECT_NE(capturedOutput.find("b"), std::string::npos);
}

TEST_P(WorkspaceScopeTest, WhoFiltersNarginNargout)
{
    eval(R"(
        function r = test_who2()
            x = 42;
            who;
            r = 1;
        end
    )");
    eval("test_who2();");
    EXPECT_NE(capturedOutput.find("x"), std::string::npos);
    EXPECT_EQ(capturedOutput.find("nargin"), std::string::npos);
    EXPECT_EQ(capturedOutput.find("nargout"), std::string::npos);
}

// ── whos ──

TEST_P(WorkspaceScopeTest, WhosInsideFunction)
{
    eval(R"(
        function r = test_whos()
            x = [1 2 3];
            whos;
            r = 1;
        end
    )");
    eval("test_whos();");
    EXPECT_NE(capturedOutput.find("x"), std::string::npos);
    EXPECT_NE(capturedOutput.find("1x3"), std::string::npos);
    EXPECT_EQ(capturedOutput.find("nargin"), std::string::npos);
    EXPECT_EQ(capturedOutput.find("nargout"), std::string::npos);
}

// ── clear all ──

TEST_P(WorkspaceScopeTest, ClearAllInsideFunction)
{
    eval(R"(
        function r = test_clearall()
            x = 42;
            y = 99;
            clear all;
            r = exist('x') + exist('y');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_clearall();"), 0.0);
}

TEST_P(WorkspaceScopeTest, WhoWithArgs)
{
    eval(R"(
        function r = test_who_args()
            a = 1;
            b = 2;
            c = 3;
            who a c;
            r = 1;
        end
    )");
    capturedOutput.clear();
    eval("test_who_args();");
    EXPECT_NE(capturedOutput.find("a  "), std::string::npos);
    EXPECT_NE(capturedOutput.find("c  "), std::string::npos);
    // "b  " should not appear (but "b" exists in "variables")
    EXPECT_EQ(capturedOutput.find("b  "), std::string::npos);
}

TEST_P(WorkspaceScopeTest, WhosWithArgs)
{
    eval(R"(
        function r = test_whos_args()
            x = [1 2 3];
            y = 42;
            whos x;
            r = 1;
        end
    )");
    capturedOutput.clear();
    eval("test_whos_args();");
    EXPECT_NE(capturedOutput.find("x"), std::string::npos);
    EXPECT_NE(capturedOutput.find("1x3"), std::string::npos);
    // "  y" with leading spaces = whos entry. Should not appear.
    EXPECT_EQ(capturedOutput.find("  y"), std::string::npos);
}

INSTANTIATE_DUAL(WorkspaceScopeTest);

// ============================================================
// exist with type filter
// ============================================================

class ExistFilterTest : public DualEngineTest
{};

TEST_P(ExistFilterTest, ExistVarFilter)
{
    eval(R"(
        function r = test_exist_var()
            x = 42;
            r = exist('x', 'var');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_exist_var();"), 1.0);
}

TEST_P(ExistFilterTest, ExistVarFilterMissing)
{
    eval(R"(
        function r = test_exist_var2()
            r = exist('zzz', 'var');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_exist_var2();"), 0.0);
}

TEST_P(ExistFilterTest, ExistBuiltinFilter)
{
    eval(R"(
        function r = test_exist_builtin()
            r = exist('sin', 'builtin');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_exist_builtin();"), 5.0);
}

TEST_P(ExistFilterTest, ExistBuiltinNotVar)
{
    eval(R"(
        function r = test_exist_bv()
            r = exist('sin', 'var');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_exist_bv();"), 0.0);
}

TEST_P(ExistFilterTest, ExistVarNotBuiltin)
{
    eval(R"(
        function r = test_exist_vb()
            x = 42;
            r = exist('x', 'builtin');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_exist_vb();"), 0.0);
}

INSTANTIATE_DUAL(ExistFilterTest);

// ============================================================
// clear global
// ============================================================

class ClearGlobalTest : public DualEngineTest
{};

TEST_P(ClearGlobalTest, ClearGlobalVariable)
{
    eval("global g; g = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("g;"), 42.0);
    eval("clear global g;");
    // After clearing, exist should return 0
    EXPECT_DOUBLE_EQ(evalScalar("exist('g');"), 0.0);
}

TEST_P(ClearGlobalTest, ClearLocalDoesNotSeeGlobal)
{
    // Global x exists, function clears local x — exist should return 0
    eval("x = 100;");
    eval(R"(
        function r = test_scope()
            x = 42;
            clear x;
            r = exist('x');
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_scope();"), 0.0);
    // Global x should still be there
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 100.0);
}

INSTANTIATE_DUAL(ClearGlobalTest);

// ============================================================
// Function scope isolation (MATLAB semantics)
// ============================================================

class ScopeIsolationTest : public DualEngineTest
{};

TEST_P(ScopeIsolationTest, FunctionCannotSeeGlobalVars)
{
    eval("x = 100;");
    eval(R"(
        function r = test_iso()
            r = exist('x');
        end
    )");
    // Function should NOT see top-level x
    EXPECT_DOUBLE_EQ(evalScalar("test_iso();"), 0.0);
    // Top-level x still exists
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 100.0);
}

TEST_P(ScopeIsolationTest, FunctionSeesConstants)
{
    eval(R"(
        function r = test_const()
            r = pi;
        end
    )");
    EXPECT_NEAR(evalScalar("test_const();"), 3.14159265358979, 1e-10);
}

TEST_P(ScopeIsolationTest, ClosureCapturesParentScope)
{
    eval("a = 10; f = @(x) x + a;");
    EXPECT_DOUBLE_EQ(evalScalar("f(5);"), 15.0);
}

TEST_P(ScopeIsolationTest, ClosureDoesNotSeeNewGlobals)
{
    eval("a = 10; f = @(x) x + a;");
    eval("a = 999;");
    // Closure captured a=10 at creation time (snapshot)
    EXPECT_DOUBLE_EQ(evalScalar("f(5);"), 15.0);
}

TEST_P(ScopeIsolationTest, FunctionCannotReadGlobalVar)
{
    eval("x = 100;");
    eval(R"(
        function r = test_read_global()
            r = x;
        end
    )");
    // Function defined OK, but calling it throws — x not in function scope
    EXPECT_THROW(eval("test_read_global();"), std::exception);
}

TEST_P(ScopeIsolationTest, FunctionCannotReadGlobalVarSameEval)
{
    // Same eval — function def + call
    EXPECT_THROW(eval(R"(
        x = 100;
        function r = test_rg2()
            r = x;
        end
        test_rg2();
    )"),
                 std::exception);
}

TEST_P(ScopeIsolationTest, FuncBodyVarNotLeakedToScript)
{
    // Variable 'internal_var' only exists inside function body.
    // preImportGlobals should NOT import it into top-level script.
    eval("internal_var = 999;");
    eval(R"(
        function r = uses_internal()
            internal_var = 42;
            r = internal_var;
        end
        result = uses_internal();
    )");
    // Function should use its own local internal_var, not top-level
    EXPECT_DOUBLE_EQ(evalScalar("result;"), 42.0);
    // Top-level internal_var should be unchanged
    EXPECT_DOUBLE_EQ(evalScalar("internal_var;"), 999.0);
}

INSTANTIATE_DUAL(ScopeIsolationTest);

// ============================================================
// Clear + subsequent variable creation (export semantics)
// ============================================================

class ClearExportTest : public DualEngineTest
{};

TEST_P(ClearExportTest, ClearThenCreateVars)
{
    // Standard MATLAB pattern: clear at top of script, then create vars
    eval("x = 999;"); // pre-existing
    eval("clear; y = 42; z = 100;");
    // x should be gone, y and z should exist
    EXPECT_DOUBLE_EQ(evalScalar("y;"), 42.0);
    EXPECT_DOUBLE_EQ(evalScalar("z;"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('x');"), 0.0);
}

TEST_P(ClearExportTest, ClearAllThenCreateVars)
{
    eval("a = 1; b = 2;");
    eval("clear all; c = 3;");
    EXPECT_DOUBLE_EQ(evalScalar("c;"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('a');"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('b');"), 0.0);
}

TEST_P(ClearExportTest, EmptyMatrixSurvivesClear)
{
    // A=[] should export correctly — it's a valid value, not "cleared"
    eval("A = []; B = 5;");
    EXPECT_EQ(getVarPtr("A")->numel(), 0u);
    EXPECT_DOUBLE_EQ(evalScalar("B;"), 5.0);
}

TEST_P(ClearExportTest, EmptyMatrixAfterClear)
{
    eval("x = 999;");
    eval("clear; A = []; B = 10;");
    EXPECT_EQ(getVarPtr("A")->numel(), 0u);
    EXPECT_DOUBLE_EQ(evalScalar("B;"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('x');"), 0.0);
}

TEST_P(ClearExportTest, ClearSpecificThenCreate)
{
    eval("x = 1; y = 2;");
    eval("clear x; z = 3;");
    EXPECT_DOUBLE_EQ(evalScalar("y;"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("z;"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('x');"), 0.0);
}

TEST_P(ClearExportTest, ConstantsSurviveClear)
{
    eval("clear;");
    EXPECT_NEAR(evalScalar("pi;"), 3.14159265358979, 1e-10);
    EXPECT_DOUBLE_EQ(evalScalar("true;"), 1.0);
}

TEST_P(ClearExportTest, FunctionReturnUninitialized)
{
    // Function that doesn't assign return var — should return empty
    eval(R"(
        function r = test_uninit()
        end
    )");
    MValue r = eval("test_uninit();");
    EXPECT_TRUE(r.isEmpty());
}

// ── clear in middle of script ──

TEST_P(ClearExportTest, ClearInMiddleOfScript)
{
    eval("a = 1; b = 2; clear; c = 3; d = 4;");
    EXPECT_DOUBLE_EQ(evalScalar("c;"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("d;"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('a');"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('b');"), 0.0);
}

TEST_P(ClearExportTest, ClearSpecificInMiddle)
{
    eval("a = 1; b = 2; c = 3; clear b; d = a + c;");
    EXPECT_DOUBLE_EQ(evalScalar("a;"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c;"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("d;"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('b');"), 0.0);
}

TEST_P(ClearExportTest, ClearMultipleVars)
{
    eval("a = 1; b = 2; c = 3; d = 4; clear a c;");
    EXPECT_DOUBLE_EQ(evalScalar("b;"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("d;"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('a');"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('c');"), 0.0);
}

TEST_P(ClearExportTest, ClearThenReassign)
{
    eval("x = 10; clear x; x = 20;");
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 20.0);
}

TEST_P(ClearExportTest, ClearAllThenReassignSameName)
{
    eval("x = 10; clear all; x = 99;");
    EXPECT_DOUBLE_EQ(evalScalar("x;"), 99.0);
}

// ── clear inside functions ──

TEST_P(ClearExportTest, ClearInsideFunctionLocalOnly)
{
    eval("g = 100;");
    eval(R"(
        function r = test_func_clear()
            a = 1;
            b = 2;
            clear a;
            r = exist('a') * 10 + b;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_func_clear();"), 2.0);
    // global g untouched
    EXPECT_DOUBLE_EQ(evalScalar("g;"), 100.0);
}

TEST_P(ClearExportTest, ClearAllInsideFunctionThenCreate)
{
    eval(R"(
        function r = test_func_clearall()
            a = 1;
            b = 2;
            clear all;
            c = 3;
            r = exist('a') * 100 + exist('b') * 10 + c;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_func_clearall();"), 3.0);
}

TEST_P(ClearExportTest, ClearFunctionCallStyle)
{
    eval("a = 1; b = 2; c = 3; clear('b');");
    EXPECT_DOUBLE_EQ(evalScalar("a;"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c;"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('b');"), 0.0);
}

TEST_P(ClearExportTest, ClearFunctionCallInsideFunc)
{
    eval(R"(
        function r = test_func_clear2()
            x = 10;
            y = 20;
            clear('x');
            r = exist('x') * 100 + y;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_func_clear2();"), 20.0);
}

TEST_P(ClearExportTest, ClearWithRuntimeName)
{
    eval(R"(
        function r = test_dyn_clear()
            x = 10;
            y = 20;
            name = 'x';
            clear(name);
            r = exist('x') * 100 + y;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("test_dyn_clear();"), 20.0);
}

// ── clear in loops ──

TEST_P(ClearExportTest, ClearInsideLoop)
{
    eval(R"(
        function r = test_loop_clear()
            total = 0;
            for k = 1:3
                tmp = k * 10;
                total = total + tmp;
                clear tmp;
            end
            r = total + exist('tmp') * 1000;
        end
    )");
    // total = 10+20+30 = 60, tmp cleared so exist=0
    EXPECT_DOUBLE_EQ(evalScalar("test_loop_clear();"), 60.0);
}

// ── clear preserves constants ──

TEST_P(ClearExportTest, ClearInsideFunctionConstantsVisible)
{
    eval(R"(
        function r = test_const_clear()
            clear all;
            r = pi;
        end
    )");
    EXPECT_NEAR(evalScalar("test_const_clear();"), 3.14159265358979, 1e-10);
}

// ── multiple clears ──

TEST_P(ClearExportTest, MultipleClearsInSequence)
{
    eval("a = 1; clear a; b = 2; clear b; c = 3;");
    EXPECT_DOUBLE_EQ(evalScalar("c;"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('a');"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('b');"), 0.0);
}

INSTANTIATE_DUAL(ClearExportTest);

// ============================================================
// Bare function calls (no parentheses) in expression context
// ============================================================

class BareFuncCallTest : public DualEngineTest
{};

TEST_P(BareFuncCallTest, TicTocBasic)
{
    eval("tic; t = toc;");
    double t = evalScalar("t;");
    EXPECT_GE(t, 0.0);
    EXPECT_LT(t, 5.0); // should be near-instant
}

TEST_P(BareFuncCallTest, TocWithoutParens)
{
    // t1 = toc should work the same as t1 = toc()
    eval("tic;");
    eval("t1 = toc;");
    double t1 = evalScalar("t1;");
    EXPECT_GE(t1, 0.0);
}

TEST_P(BareFuncCallTest, TicReturnValue)
{
    eval("id = tic;");
    double id = evalScalar("id;");
    EXPECT_GT(id, 0.0); // timestamp in microseconds
}

TEST_P(BareFuncCallTest, RandWithoutParens)
{
    // r = rand should call rand() and return scalar
    eval("r = rand;");
    double r = evalScalar("r;");
    EXPECT_GE(r, 0.0);
    EXPECT_LE(r, 1.0);
}

TEST_P(BareFuncCallTest, BareFuncInExpression)
{
    // pi is a constant, not a function — should still work
    eval("x = pi + 1;");
    EXPECT_NEAR(evalScalar("x;"), 4.14159265358979, 1e-10);
}

TEST_P(BareFuncCallTest, TicTocInFunction)
{
    eval(R"(
        function t = test_tictoc()
            tic;
            t = toc;
        end
    )");
    double t = evalScalar("test_tictoc();");
    EXPECT_GE(t, 0.0);
    EXPECT_LT(t, 5.0);
}

INSTANTIATE_DUAL(BareFuncCallTest);

// ============================================================
// Function definition after clear in same eval
// ============================================================

class ClearAndFuncTest : public DualEngineTest
{};

TEST_P(ClearAndFuncTest, ClearThenDefineAndCall)
{
    eval(R"(
        clear

        function y = add_one(x)
            y = x + 1;
        end

        r = add_one(5);
    )");
    EXPECT_DOUBLE_EQ(evalScalar("r;"), 6.0);
}

TEST_P(ClearAndFuncTest, ClearThenDefineAndCallInLoop)
{
    eval(R"(
        clear

        function y = inc(x)
            y = x + 1;
        end

        v = 0;
        for i = 1:10
            v = inc(v);
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("v;"), 10.0);
}

TEST_P(ClearAndFuncTest, DefineAndCallWithoutClear)
{
    eval(R"(
        function y = double_it(x)
            y = x * 2;
        end

        r = double_it(7);
    )");
    EXPECT_DOUBLE_EQ(evalScalar("r;"), 14.0);
}

INSTANTIATE_DUAL(ClearAndFuncTest);

// ============================================================
// nargout propagation — statement vs expression context
// ============================================================

class NargoutTest : public DualEngineTest
{};

TEST_P(NargoutTest, TicStatementNoOutput)
{
    // tic; as statement should NOT display anything
    capturedOutput.clear();
    eval("tic;");
    EXPECT_EQ(capturedOutput.find("ans"), std::string::npos);
}

TEST_P(NargoutTest, TicExpressionReturnsValue)
{
    // id = tic should return a timestamp
    eval("id = tic;");
    double id = evalScalar("id;");
    EXPECT_GT(id, 0.0);
}

TEST_P(NargoutTest, TocStatementPrintsElapsed)
{
    // toc as statement should print "Elapsed time is..." not "ans = ..."
    eval("tic;");
    capturedOutput.clear();
    eval("toc;");
    EXPECT_NE(capturedOutput.find("Elapsed time"), std::string::npos);
    EXPECT_EQ(capturedOutput.find("ans"), std::string::npos);
}

TEST_P(NargoutTest, TocExpressionReturnsScalar)
{
    eval("tic;");
    eval("t = toc;");
    double t = evalScalar("t;");
    EXPECT_GE(t, 0.0);
}

TEST_P(NargoutTest, FprintfNoAnsOutput)
{
    // fprintf as statement should NOT display ans = []
    capturedOutput.clear();
    eval("fprintf('hello\\n');");
    EXPECT_NE(capturedOutput.find("hello"), std::string::npos);
    EXPECT_EQ(capturedOutput.find("ans"), std::string::npos);
}

TEST_P(NargoutTest, FunctionCallStatementNoAns)
{
    // User function called as statement — no ans display
    eval(R"(
        function do_nothing()
        end
    )");
    capturedOutput.clear();
    eval("do_nothing();");
    EXPECT_EQ(capturedOutput.find("ans"), std::string::npos);
}

TEST_P(NargoutTest, FunctionCallExpressionReturns)
{
    eval(R"(
        function r = get_value()
            r = 42;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("get_value();"), 42.0);
}

TEST_P(NargoutTest, DispStatementNoAns)
{
    capturedOutput.clear();
    eval("disp(42);");
    EXPECT_NE(capturedOutput.find("42"), std::string::npos);
    EXPECT_EQ(capturedOutput.find("ans"), std::string::npos);
}

INSTANTIATE_DUAL(NargoutTest);

// ============================================================
// MValue::colonRange / horzcat / vertcat — shared operations
// ============================================================

class SharedOpsTest : public DualEngineTest
{};

// ── colonRange ──────────────────────────────────────────────

TEST_P(SharedOpsTest, ColonRangeUnitStep)
{
    eval("v = 1:5;");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 5u);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_DOUBLE_EQ(v->doubleData()[i], 1.0 + i);
}

TEST_P(SharedOpsTest, ColonRangeFractionalStep)
{
    eval("v = 0:0.5:2;");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 5u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 2.0);
}

TEST_P(SharedOpsTest, ColonRangeNegativeStep)
{
    eval("v = 10:-3:1;");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 4u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 10.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 1.0);
}

TEST_P(SharedOpsTest, ColonRangeEmptyResult)
{
    eval("v = 5:1;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 0u);
}

TEST_P(SharedOpsTest, ColonRangeZeroStepError)
{
    auto r = engine.evalSafe("v = 1:0:10;");
    EXPECT_FALSE(r.ok);
}

TEST_P(SharedOpsTest, ColonRangeLastElementCorrection)
{
    // 0:0.3:1 — last element should be exactly 0.9, not 0.9000000000000001
    eval("v = 0:0.3:1;");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 4u);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 0.9);
}

TEST_P(SharedOpsTest, ColonRangeSingleElement)
{
    eval("v = 5:5;");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 1u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 5.0);
}

// ── horzcat ─────────────────────────────────────────────────

TEST_P(SharedOpsTest, HorzcatScalars)
{
    eval("v = [1, 2, 3];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 1u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 3.0);
}

TEST_P(SharedOpsTest, HorzcatVectors)
{
    eval("a = [1 2]; b = [3 4 5]; v = [a, b];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 5u);
    EXPECT_DOUBLE_EQ(v->doubleData()[4], 5.0);
}

TEST_P(SharedOpsTest, HorzcatMatrices)
{
    eval("A = [1; 2]; B = [3; 4]; C = [A, B];");
    auto *v = getVarPtr("C");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_DOUBLE_EQ((*v)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*v)(0, 1), 3.0);
    EXPECT_DOUBLE_EQ((*v)(1, 1), 4.0);
}

TEST_P(SharedOpsTest, HorzcatDimensionMismatchError)
{
    auto r = engine.evalSafe("A = [1 2; 3 4]; B = [5; 6; 7]; C = [A, B];");
    EXPECT_FALSE(r.ok);
}

TEST_P(SharedOpsTest, HorzcatStrings)
{
    eval("s = ['hello', ' ', 'world'];");
    auto *v = getVarPtr("s");
    EXPECT_EQ(v->toString(), "hello world");
}

TEST_P(SharedOpsTest, HorzcatWithEmpty)
{
    eval("v = [1 2 [] 3];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 3.0);
}

// ── vertcat ─────────────────────────────────────────────────

TEST_P(SharedOpsTest, VertcatScalars)
{
    eval("v = [1; 2; 3];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 1u);
}

TEST_P(SharedOpsTest, VertcatRowVectors)
{
    eval("A = [1 2 3; 4 5 6];");
    auto *v = getVarPtr("A");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_DOUBLE_EQ((*v)(1, 2), 6.0);
}

TEST_P(SharedOpsTest, VertcatMatrices)
{
    eval("A = [1 2; 3 4]; B = [5 6; 7 8]; C = [A; B];");
    auto *v = getVarPtr("C");
    EXPECT_EQ(rows(*v), 4u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_DOUBLE_EQ((*v)(2, 0), 5.0);
    EXPECT_DOUBLE_EQ((*v)(3, 1), 8.0);
}

TEST_P(SharedOpsTest, VertcatDimensionMismatchError)
{
    auto r = engine.evalSafe("x = [1 2; 3 4 5];");
    EXPECT_FALSE(r.ok);
}

TEST_P(SharedOpsTest, VertcatScalarAndVector)
{
    eval("v = [1 2 3; 4 5 6; 7 8 9];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 3u);
    EXPECT_DOUBLE_EQ((*v)(2, 2), 9.0);
}

// ── Combined horzcat + vertcat ──────────────────────────────

TEST_P(SharedOpsTest, MixedConcatBuildMatrix)
{
    // [1 2; 3 4] uses horzcat per row then vertcat
    eval("M = [1 2; 3 4];");
    auto *v = getVarPtr("M");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_DOUBLE_EQ((*v)(0, 0), 1.0);
    EXPECT_DOUBLE_EQ((*v)(1, 1), 4.0);
}

TEST_P(SharedOpsTest, ConcatWithColonRange)
{
    eval("v = [1:3, 10:12];");
    auto *v = getVarPtr("v");
    ASSERT_EQ(v->numel(), 6u);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 10.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[5], 12.0);
}

TEST_P(SharedOpsTest, ConcatPreservesColumnMajor)
{
    // Column-major layout: [1 3; 2 4] stored as [1,2,3,4]
    eval("M = [1 3; 2 4];");
    auto *v = getVarPtr("M");
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 3.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[3], 4.0);
}

// ── Complex concat ──────────────────────────────────────────

TEST_P(SharedOpsTest, HorzcatComplex)
{
    eval("v = [1+2i, 3+4i];");
    auto *v = getVarPtr("v");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(v->numel(), 2u);
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(v->complexData()[0].imag(), 2.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].real(), 3.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].imag(), 4.0);
}

TEST_P(SharedOpsTest, VertcatComplex)
{
    eval("v = [1+2i; 3+4i];");
    auto *v = getVarPtr("v");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 1u);
}

TEST_P(SharedOpsTest, MixedDoubleComplexPromotes)
{
    // Mixing double and complex → result should be complex
    eval("v = [1, 2+3i, 4];");
    auto *v = getVarPtr("v");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(v->complexData()[0].imag(), 0.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].real(), 2.0);
    EXPECT_DOUBLE_EQ(v->complexData()[1].imag(), 3.0);
}

TEST_P(SharedOpsTest, VertcatMixedDoubleComplex)
{
    eval("A = [1 2; 3 4]; B = [5+1i 6+2i]; C = [A; B];");
    auto *v = getVarPtr("C");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(rows(*v), 3u);
    EXPECT_EQ(cols(*v), 2u);
    // A elements promoted to complex
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(v->complexData()[0].imag(), 0.0);
}

// ── Logical concat ──────────────────────────────────────────

TEST_P(SharedOpsTest, HorzcatLogicalStaysLogical)
{
    eval("v = [true, false, true];");
    auto *v = getVarPtr("v");
    // MATLAB: [true, false, true] stays logical
    EXPECT_TRUE(v->isLogical());
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_EQ(v->logicalData()[0], 1);
    EXPECT_EQ(v->logicalData()[1], 0);
    EXPECT_EQ(v->logicalData()[2], 1);
}

TEST_P(SharedOpsTest, MixedLogicalDoubleConcat)
{
    eval("v = [true, 5, false];");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->type(), mlab::MType::DOUBLE);
    EXPECT_DOUBLE_EQ(v->doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[1], 5.0);
    EXPECT_DOUBLE_EQ(v->doubleData()[2], 0.0);
}

// ── 3D concat ───────────────────────────────────────────────

TEST_P(SharedOpsTest, Vertcat3DArrays)
{
    // Create two 1×2×2 arrays, vertcat → 2×2×2
    eval(R"(
        A = zeros(1,2,2); A(1,1,1) = 1; A(1,2,1) = 2; A(1,1,2) = 3; A(1,2,2) = 4;
        B = zeros(1,2,2); B(1,1,1) = 5; B(1,2,1) = 6; B(1,1,2) = 7; B(1,2,2) = 8;
        C = vertcat(A, B);
    )");
    auto *v = getVarPtr("C");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_TRUE(v->dims().is3D());
    EXPECT_EQ(v->dims().pages(), 2u);
}

TEST_P(SharedOpsTest, Horzcat3DArrays)
{
    // Create two 2×1×2 arrays, horzcat → 2×2×2
    eval(R"(
        A = zeros(2,1,2); A(1,1,1) = 1; A(2,1,1) = 2; A(1,1,2) = 3; A(2,1,2) = 4;
        B = zeros(2,1,2); B(1,1,1) = 5; B(2,1,1) = 6; B(1,1,2) = 7; B(2,1,2) = 8;
        C = horzcat(A, B);
    )");
    auto *v = getVarPtr("C");
    EXPECT_EQ(rows(*v), 2u);
    EXPECT_EQ(cols(*v), 2u);
    EXPECT_TRUE(v->dims().is3D());
    EXPECT_EQ(v->dims().pages(), 2u);
}

TEST_P(SharedOpsTest, Vertcat3DPagesMismatchError)
{
    auto r = engine.evalSafe(R"(
        A = zeros(1,2,2);
        B = zeros(1,2,3);
        C = vertcat(A, B);
    )");
    EXPECT_FALSE(r.ok);
}

// ── Type-preserving indexing ─────────────────────────────────

TEST_P(SharedOpsTest, IndexComplexArray1D)
{
    // X(1:3) where X is complex
    eval("X = [1+2i, 3+4i, 5+6i, 7+8i]; Y = X(1:3);");
    auto *v = getVarPtr("Y");
    EXPECT_TRUE(v->isComplex());
    EXPECT_EQ(v->numel(), 3u);
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(v->complexData()[2].imag(), 6.0);
}

TEST_P(SharedOpsTest, IndexComplexScalar)
{
    eval("X = [1+2i, 3+4i]; Y = X(2);");
    auto *v = getVarPtr("Y");
    EXPECT_TRUE(v->isComplex());
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 3.0);
    EXPECT_DOUBLE_EQ(v->complexData()[0].imag(), 4.0);
}

TEST_P(SharedOpsTest, IndexComplexArray2D)
{
    eval("X = [1+1i 2+2i; 3+3i 4+4i]; Y = X(1, 2);");
    auto *v = getVarPtr("Y");
    EXPECT_TRUE(v->isComplex());
    EXPECT_DOUBLE_EQ(v->complexData()[0].real(), 2.0);
}

TEST_P(SharedOpsTest, IndexLogicalArray)
{
    eval("X = [true false true false]; Y = X(2:3);");
    auto *v = getVarPtr("Y");
    EXPECT_TRUE(v->isLogical());
    EXPECT_EQ(v->numel(), 2u);
}

TEST_P(SharedOpsTest, FFTSpectrumSlicing)
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
    EXPECT_EQ(mag->type(), mlab::MType::DOUBLE);
    EXPECT_EQ(mag->numel(), 129u);
}

// ── Cell () indexing ────────────────────────────────────────

TEST_P(SharedOpsTest, CellParenScalarIndex)
{
    // c(2) on cell returns a 1x1 sub-cell containing the content
    eval("c = {10, 'hello', [1 2 3]}; r = c(2);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isCell());
    EXPECT_EQ(r->numel(), 1u);
    EXPECT_TRUE(r->cellAt(0).isChar());
    EXPECT_EQ(r->cellAt(0).toString(), "hello");
}

TEST_P(SharedOpsTest, CellParenVectorIndex)
{
    // c([1 3]) returns a sub-cell
    eval("c = {10, 'hello', [1 2 3]}; r = c([1 3]);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isCell());
    EXPECT_EQ(r->numel(), 2u);
    EXPECT_DOUBLE_EQ(r->cellAt(0).toScalar(), 10.0);
    EXPECT_EQ(r->cellAt(1).numel(), 3u);
}

TEST_P(SharedOpsTest, CellCurlyIndex)
{
    eval("c = {10, 'hello', [1 2 3]}; r = c{3};");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->type(), mlab::MType::DOUBLE);
    EXPECT_EQ(r->numel(), 3u);
}

TEST_P(SharedOpsTest, CellParenIndex2D)
{
    // c(2,1) on cell returns a 1x1 sub-cell
    eval("c = {1 2; 3 4}; r = c(2, 1);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isCell());
    EXPECT_EQ(r->numel(), 1u);
    EXPECT_DOUBLE_EQ(r->cellAt(0).toScalar(), 3.0);
}

TEST_P(SharedOpsTest, CellCurlyIndex2D)
{
    eval("c = {1 2; 3 4}; r = c{1, 2};");
    auto *r = getVarPtr("r");
    EXPECT_DOUBLE_EQ(r->toScalar(), 2.0);
}

TEST_P(SharedOpsTest, CellCurlyAssign)
{
    eval("c = {0, 0, 0}; c{2} = 'world'; r = c{2};");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isChar());
    EXPECT_EQ(r->toString(), "world");
}

TEST_P(SharedOpsTest, CellParenLogicalIndex)
{
    eval("c = {10, 20, 30}; r = c([true false true]);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isCell());
    EXPECT_EQ(r->numel(), 2u);
    EXPECT_DOUBLE_EQ(r->cellAt(0).toScalar(), 10.0);
    EXPECT_DOUBLE_EQ(r->cellAt(1).toScalar(), 30.0);
}

// ── 3D indexing ────────────────────────────────────────────

TEST_P(SharedOpsTest, Index3DGetSlice)
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

TEST_P(SharedOpsTest, Index3DSetAndGet)
{
    eval(R"(
        A = zeros(2, 2, 2);
        A(1, 2, 2) = 99;
        r = A(1, 2, 2);
    )");
    auto *r = getVarPtr("r");
    EXPECT_DOUBLE_EQ(r->toScalar(), 99.0);
}

// ── Logical mask SET ─────────────────────────────────────

TEST_P(SharedOpsTest, LogicalMaskSetScalar)
{
    eval("A = [10 20 30 40 50]; A([true false true false true]) = 0;");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0), 0.0);
    EXPECT_DOUBLE_EQ((*A)(1), 20.0);
    EXPECT_DOUBLE_EQ((*A)(2), 0.0);
    EXPECT_DOUBLE_EQ((*A)(3), 40.0);
    EXPECT_DOUBLE_EQ((*A)(4), 0.0);
}

TEST_P(SharedOpsTest, LogicalMaskSetVector)
{
    eval("A = [1 2 3 4 5]; A([false true false true false]) = [99 88];");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ((*A)(0), 1.0);
    EXPECT_DOUBLE_EQ((*A)(1), 99.0);
    EXPECT_DOUBLE_EQ((*A)(2), 3.0);
    EXPECT_DOUBLE_EQ((*A)(3), 88.0);
    EXPECT_DOUBLE_EQ((*A)(4), 5.0);
}

TEST_P(SharedOpsTest, LogicalMaskSetComplex)
{
    eval("A = [1+1i, 2+2i, 3+3i]; A([true false true]) = 0;");
    auto *A = getVarPtr("A");
    EXPECT_DOUBLE_EQ(A->complexData()[0].real(), 0.0);
    EXPECT_DOUBLE_EQ(A->complexData()[1].real(), 2.0);
    EXPECT_DOUBLE_EQ(A->complexData()[2].real(), 0.0);
}

TEST_P(SharedOpsTest, LogicalMaskSetLogical)
{
    eval("A = [true true false false]; A([false false true true]) = true;");
    auto *A = getVarPtr("A");
    EXPECT_TRUE(A->isLogical());
    EXPECT_EQ(A->logicalData()[0], 1);
    EXPECT_EQ(A->logicalData()[1], 1);
    EXPECT_EQ(A->logicalData()[2], 1);
    EXPECT_EQ(A->logicalData()[3], 1);
}

// ── Logical mask DELETE ─────────────────────────────────

TEST_P(SharedOpsTest, LogicalMaskDelete)
{
    eval("A = [10 20 30 40 50]; A([true false true false false]) = [];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(A->numel(), 3u);
    EXPECT_DOUBLE_EQ(A->doubleData()[0], 20.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[1], 40.0);
    EXPECT_DOUBLE_EQ(A->doubleData()[2], 50.0);
}

TEST_P(SharedOpsTest, LogicalMaskDeleteComplex)
{
    eval("A = [1+2i, 3+4i, 5+6i]; A([false true false]) = [];");
    auto *A = getVarPtr("A");
    EXPECT_TRUE(A->isComplex());
    EXPECT_EQ(A->numel(), 2u);
    EXPECT_DOUBLE_EQ(A->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(A->complexData()[1].real(), 5.0);
}

TEST_P(SharedOpsTest, LogicalMaskDeleteChar)
{
    eval("s = 'abcde'; s([true false true false true]) = [];");
    auto *s = getVarPtr("s");
    EXPECT_TRUE(s->isChar());
    EXPECT_EQ(s->toString(), "bd");
}

TEST_P(SharedOpsTest, LogicalMaskDeleteCell)
{
    eval("c = {10, 20, 30, 40}; c([false true false true]) = [];");
    auto *c = getVarPtr("c");
    EXPECT_TRUE(c->isCell());
    EXPECT_EQ(c->numel(), 2u);
    EXPECT_DOUBLE_EQ(c->cellAt(0).toScalar(), 10.0);
    EXPECT_DOUBLE_EQ(c->cellAt(1).toScalar(), 30.0);
}

// ── Logical mask GET — CHAR (was missing) ───────────────

TEST_P(SharedOpsTest, LogicalMaskGetChar)
{
    eval("s = 'hello'; r = s([true false true false true]);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isChar());
    EXPECT_EQ(r->toString(), "hlo");
}

// ── Cell comma-separated list ───────────────────────────

TEST_P(SharedOpsTest, CellCSLBasic)
{
    eval("c = {10, 'hello', [1 2 3]}; [a, b] = c{[1 2]};");
    auto *a = getVarPtr("a");
    auto *b = getVarPtr("b");
    EXPECT_DOUBLE_EQ(a->toScalar(), 10.0);
    EXPECT_TRUE(b->isChar());
    EXPECT_EQ(b->toString(), "hello");
}

TEST_P(SharedOpsTest, CellCSLColon)
{
    eval("c = {1, 2, 3}; [x, y, z] = c{:};");
    EXPECT_DOUBLE_EQ(getVar("x"), 1.0);
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
    EXPECT_DOUBLE_EQ(getVar("z"), 3.0);
}

TEST_P(SharedOpsTest, CellCSLWithTilde)
{
    eval("c = {10, 20, 30}; [~, b, ~] = c{:};");
    EXPECT_DOUBLE_EQ(getVar("b"), 20.0);
    EXPECT_EQ(getVarPtr("~"), nullptr);
}

// ============================================================
// Compliance fix tests — COW, Inf/NaN, type correctness
// ============================================================

// --- COW: cell array sharing ---
TEST_P(SharedOpsTest, COWCellSharing)
{
    eval("a = {1, 2, 3}; b = a; a{2} = 99;");
    EXPECT_DOUBLE_EQ(getVarPtr("a")->cellAt(1).toScalar(), 99.0);
    EXPECT_DOUBLE_EQ(getVarPtr("b")->cellAt(1).toScalar(), 2.0); // b unchanged
}

// --- COW: struct sharing ---
TEST_P(SharedOpsTest, COWStructSharing)
{
    eval("s.x = 10; t = s; s.x = 99;");
    EXPECT_DOUBLE_EQ(getVarPtr("s")->field("x").toScalar(), 99.0);
    // t.x should still be 10 — not corrupted by COW violation
    EXPECT_DOUBLE_EQ(getVarPtr("t")->field("x").toScalar(), 10.0);
}

// --- COW: promoteToComplex ---
TEST_P(SharedOpsTest, COWPromoteToComplex)
{
    eval("a = [1 2 3]; b = a; a(1) = 1i;");
    EXPECT_TRUE(getVarPtr("a")->isComplex());
    EXPECT_TRUE(getVarPtr("b")->type() == mlab::MType::DOUBLE); // b stays double
    EXPECT_DOUBLE_EQ(getVarPtr("b")->doubleData()[0], 1.0);
}

// --- Inf/NaN index validation ---
TEST_P(SharedOpsTest, InfIndexError)
{
    EXPECT_THROW(eval("v = [1 2 3]; v(inf);"), std::runtime_error);
}

TEST_P(SharedOpsTest, NaNIndexError)
{
    EXPECT_THROW(eval("v = [1 2 3]; v(nan);"), std::runtime_error);
}

TEST_P(SharedOpsTest, FloatIndexError)
{
    EXPECT_THROW(eval("v = [1 2 3]; v(1.5);"), std::runtime_error);
}

TEST_P(SharedOpsTest, FloatIndexIntegerOK)
{
    // Integer-valued float should work
    EXPECT_DOUBLE_EQ(evalScalar("v = [10 20 30]; v(2.0);"), 20.0);
}

TEST_P(SharedOpsTest, NegativeIndexError)
{
    EXPECT_THROW(eval("v = [1 2 3]; v(-1);"), std::runtime_error);
}

// --- Colon Inf/NaN ---
TEST_P(SharedOpsTest, ColonInfError)
{
    EXPECT_THROW(eval("x = 1:inf;"), std::runtime_error);
}

TEST_P(SharedOpsTest, ColonNaNEmpty)
{
    eval("x = 1:nan;");
    EXPECT_TRUE(getVarPtr("x")->isEmpty());
}

// --- Scalar bounds check ---
TEST_P(SharedOpsTest, ScalarOutOfBoundsGet)
{
    EXPECT_THROW(eval("x = 5; x(2);"), std::runtime_error);
}

TEST_P(SharedOpsTest, ScalarIndexOneOK)
{
    EXPECT_DOUBLE_EQ(evalScalar("x = 42; x(1);"), 42.0);
}

// --- AND/OR/NOT return logical ---
TEST_P(SharedOpsTest, AndReturnsLogical)
{
    eval("r = 3 & 5;");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(SharedOpsTest, OrReturnsLogical)
{
    eval("r = 0 | 5;");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(SharedOpsTest, NotReturnsLogical)
{
    eval("r = ~0;");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(SharedOpsTest, NotFalseOnNonzero)
{
    eval("r = ~5;");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_FALSE(getVarPtr("r")->toBool());
}

// --- sign(NaN) ---
TEST_P(SharedOpsTest, SignNaN)
{
    eval("r = sign(nan);");
    EXPECT_TRUE(std::isnan(getVarPtr("r")->toScalar()));
}

// --- isnan/isinf return logical ---
TEST_P(SharedOpsTest, IsnanReturnsLogical)
{
    eval("r = isnan(nan);");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(SharedOpsTest, IsinfReturnsLogical)
{
    eval("r = isinf(inf);");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

// --- rem() consistency ---
TEST_P(SharedOpsTest, RemCorrect)
{
    EXPECT_DOUBLE_EQ(evalScalar("rem(7, 4);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("rem(-7, 4);"), -3.0);
}

// --- vertcat char ---
TEST_P(SharedOpsTest, VertcatChar)
{
    eval("r = ['abc'; 'def'];");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isChar());
    EXPECT_EQ(rows(*r), 2u);
    EXPECT_EQ(cols(*r), 3u);
}

// --- horzcat char + double vector ---
TEST_P(SharedOpsTest, HorzcatCharDoubleVector)
{
    eval("r = ['AB', [67 68]];");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isChar());
    EXPECT_EQ(r->toString(), "ABCD");
}

// --- indexSet size mismatch error ---
TEST_P(SharedOpsTest, IndexSetSizeMismatch)
{
    EXPECT_THROW(eval("a = [1 2 3 4 5]; a([1 2 3]) = [10 20];"), std::runtime_error);
}

// --- indexSet scalar expansion ---
TEST_P(SharedOpsTest, ScalarExpansionAssign)
{
    eval("a = [1 2 3 4 5]; a([1 3 5]) = 0;");
    auto *a = getVarPtr("a");
    expectElem(*a, 0, 0.0);
    expectElem(*a, 1, 2.0);
    expectElem(*a, 2, 0.0);
    expectElem(*a, 3, 4.0);
    expectElem(*a, 4, 0.0);
}

// --- Column vector ensureSize ---
TEST_P(SharedOpsTest, ColumnVectorGrow)
{
    eval("a = [1; 2; 3]; a(5) = 99;");
    auto *a = getVarPtr("a");
    EXPECT_EQ(rows(*a), 5u);
    EXPECT_EQ(cols(*a), 1u); // stays column vector
    expectElem(*a, 4, 99.0);
}

// --- logicalIndex bounds error ---
TEST_P(SharedOpsTest, LogicalIndexTooLong)
{
    EXPECT_THROW(eval("v = [1 2 3]; v([true true true true]);"), std::runtime_error);
}

// --- writeElem logical per-element ---
TEST_P(SharedOpsTest, LogicalWritePerElement)
{
    eval("L = [true true true]; L([1 2]) = [0 5];");
    auto *L = getVarPtr("L");
    EXPECT_EQ(L->logicalData()[0], 0); // false
    EXPECT_EQ(L->logicalData()[1], 1); // true (5 != 0)
    EXPECT_EQ(L->logicalData()[2], 1); // unchanged
}

// --- for loop on char ---
TEST_P(SharedOpsTest, ForLoopChar)
{
    eval("s = ''; for c = 'abc'; s = [s, c]; end;");
    EXPECT_EQ(getVarPtr("s")->toString(), "abc");
}

// --- for loop on logical ---
TEST_P(SharedOpsTest, ForLoopLogical)
{
    eval("n = 0; for x = [true false true]; n = n + x; end;");
    EXPECT_DOUBLE_EQ(getVar("n"), 2.0);
}

// --- switch non-scalar ---
TEST_P(SharedOpsTest, SwitchMatrix)
{
    eval("v = [1 2 3]; switch v; case [1 2 3]; r = 1; case [4 5 6]; r = 2; otherwise; r = 0; end;");
    EXPECT_DOUBLE_EQ(getVar("r"), 1.0);
}

TEST_P(SharedOpsTest, SwitchString)
{
    eval("switch 'hello'; case 'world'; r = 1; case 'hello'; r = 2; otherwise; r = 0; end;");
    EXPECT_DOUBLE_EQ(getVar("r"), 2.0);
}

// --- toBool non-scalar logical ---
TEST_P(SharedOpsTest, ToBoolLogicalArray)
{
    eval("if [true true true]; r = 1; else; r = 0; end;");
    EXPECT_DOUBLE_EQ(getVar("r"), 1.0);
    eval("if [true false true]; r = 1; else; r = 0; end;");
    EXPECT_DOUBLE_EQ(getVar("r"), 0.0);
}

// --- indexDelete shape ---
TEST_P(SharedOpsTest, DeleteOnMatrixProducesRow)
{
    eval("A = [1 2; 3 4]; A([1 2 3]) = [];");
    auto *A = getVarPtr("A");
    EXPECT_EQ(rows(*A), 1u);
    EXPECT_EQ(cols(*A), 1u);
    expectElem(*A, 0, 4.0);
}

// ============================================================
// Empty matrix operations
// ============================================================

TEST_P(SharedOpsTest, EmptyPlusEmpty)
{
    eval("r = [] + [];");
    EXPECT_TRUE(getVarPtr("r")->isEmpty());
}

TEST_P(SharedOpsTest, EmptyPlusScalar)
{
    eval("r = [] + 1;");
    EXPECT_TRUE(getVarPtr("r")->isEmpty());
}

TEST_P(SharedOpsTest, EmptyTimesScalar)
{
    eval("e = []; r = e;");
    EXPECT_TRUE(getVarPtr("r")->isEmpty());
}

TEST_P(SharedOpsTest, EmptyMinusEmpty)
{
    eval("a = []; r = a + a;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->numel(), 0u);
}

TEST_P(SharedOpsTest, SizeEmpty)
{
    EXPECT_DOUBLE_EQ(evalScalar("size([], 1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size([], 2);"), 0.0);
}

TEST_P(SharedOpsTest, LengthEmpty)
{
    EXPECT_DOUBLE_EQ(evalScalar("length([]);"), 0.0);
}

TEST_P(SharedOpsTest, IsemptyTrue)
{
    eval("r = isempty([]);");
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(SharedOpsTest, IsemptyFalse)
{
    eval("r = isempty(5);");
    EXPECT_FALSE(getVarPtr("r")->toBool());
}

TEST_P(SharedOpsTest, ZerosEmptyDim)
{
    eval("r = zeros(0, 3);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(rows(*r), 0u);
    EXPECT_EQ(cols(*r), 3u);
    EXPECT_EQ(r->numel(), 0u);
}

TEST_P(SharedOpsTest, ForOverEmpty)
{
    eval("n = 0; for i = []; n = n + 1; end;");
    EXPECT_DOUBLE_EQ(getVar("n"), 0.0); // body never executes
}

// ============================================================
// Type coercion tests
// ============================================================

TEST_P(SharedOpsTest, CharPlusDouble)
{
    // 'A' + 1 → double(66)  (MATLAB promotes char to double for arithmetic)
    eval("r = 'A' + 1;");
    EXPECT_DOUBLE_EQ(getVarPtr("r")->toScalar(), 66.0);
}

TEST_P(SharedOpsTest, NaNComparisons)
{
    // NaN == NaN → false
    eval("r = nan == nan;");
    auto *r = getVarPtr("r");
    EXPECT_FALSE(r->toBool());

    // NaN ~= NaN → true
    eval("r = nan ~= nan;");
    r = getVarPtr("r");
    EXPECT_TRUE(r->toBool());
}

TEST_P(SharedOpsTest, InfComparisons)
{
    eval("r = inf > 1e308;");
    EXPECT_TRUE(getVarPtr("r")->toBool());

    eval("r = -inf < -1e308;");
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

// --- Complex arithmetic ---
TEST_P(SharedOpsTest, ComplexMultiply)
{
    // (2+3i) * (4-1i) = 8-2i+12i-3i^2 = 8+10i+3 = 11+10i
    eval("r = (2+3i) * (4-1i);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isComplex());
    auto c = r->toComplex();
    EXPECT_DOUBLE_EQ(c.real(), 11.0);
    EXPECT_DOUBLE_EQ(c.imag(), 10.0);
}

TEST_P(SharedOpsTest, ComplexDivide)
{
    // (4+2i) / (1+1i) = (4+2i)(1-1i) / (1+1) = (4-4i+2i-2i^2)/2 = (6-2i)/2 = 3-1i
    eval("r = (4+2i) / (1+1i);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isComplex());
    auto c = r->toComplex();
    EXPECT_DOUBLE_EQ(c.real(), 3.0);
    EXPECT_DOUBLE_EQ(c.imag(), -1.0);
}

TEST_P(SharedOpsTest, ComplexPower)
{
    // (1+1i)^2 = 1+2i+i^2 = 2i
    eval("r = (1+1i)^2;");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isComplex());
    auto c = r->toComplex();
    EXPECT_NEAR(c.real(), 0.0, 1e-10);
    EXPECT_NEAR(c.imag(), 2.0, 1e-10);
}

TEST_P(SharedOpsTest, ComplexEquality)
{
    eval("r = (1+2i) == (1+2i);");
    EXPECT_TRUE(getVarPtr("r")->toBool());

    eval("r = (1+2i) == (1+3i);");
    EXPECT_FALSE(getVarPtr("r")->toBool());
}

// --- Type promotion in assignment ---
TEST_P(SharedOpsTest, DoubleToComplexPromotion)
{
    eval("a = [1 2 3]; a(2) = 1+2i;");
    auto *a = getVarPtr("a");
    EXPECT_TRUE(a->isComplex());
    auto c = a->complexData()[1];
    EXPECT_DOUBLE_EQ(c.real(), 1.0);
    EXPECT_DOUBLE_EQ(c.imag(), 2.0);
    // Original elements preserved
    EXPECT_DOUBLE_EQ(a->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(a->complexData()[0].imag(), 0.0);
}

// --- Scalar expansion in 2D assignment ---
TEST_P(SharedOpsTest, ScalarExpansion2D)
{
    eval("A = zeros(3, 3); A(:, 2) = 7;");
    auto *A = getVarPtr("A");
    expectElem2D(*A, 0, 1, 7.0);
    expectElem2D(*A, 1, 1, 7.0);
    expectElem2D(*A, 2, 1, 7.0);
    expectElem2D(*A, 0, 0, 0.0); // other cols unchanged
}

// --- Multi-return with param overlap ---
TEST_P(SharedOpsTest, MultiReturnParamOverlap)
{
    eval(R"(
        function [b, a] = swap(a, b)
            % return names overlap with params in reverse order
        end
        [x, y] = swap(10, 20);
    )");
    EXPECT_DOUBLE_EQ(getVar("x"), 20.0);
    EXPECT_DOUBLE_EQ(getVar("y"), 10.0);
}

// --- end keyword in 2D ---
TEST_P(SharedOpsTest, EndKeyword2D)
{
    eval("A = [1 2 3; 4 5 6]; r = A(end, end);");
    EXPECT_DOUBLE_EQ(getVar("r"), 6.0);
}

TEST_P(SharedOpsTest, EndKeyword2DRange)
{
    eval("A = [1 2 3; 4 5 6]; r = A(end, :);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(r->numel(), 3u);
    expectElem(*r, 0, 4.0);
    expectElem(*r, 1, 5.0);
    expectElem(*r, 2, 6.0);
}

// --- Auto-grow on set ---
TEST_P(SharedOpsTest, AutoGrowRowVector)
{
    eval("v = [1 2 3]; v(5) = 99;");
    auto *v = getVarPtr("v");
    EXPECT_EQ(v->numel(), 5u);
    expectElem(*v, 3, 0.0); // zero-filled gap
    expectElem(*v, 4, 99.0);
}

// --- Char space-fill on resize ---
TEST_P(SharedOpsTest, CharSpaceFill)
{
    eval("s = 'ab'; s(5) = 'z';");
    auto *s = getVarPtr("s");
    EXPECT_EQ(s->toString(), "ab  z");
}

// ============================================================
// Complex array comparison tests
// ============================================================

TEST_P(SharedOpsTest, ComplexArrayEq)
{
    eval("a = [1+2i, 3+4i]; b = [1+2i, 3+0i]; r = a == b;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->numel(), 2u);
    EXPECT_EQ(r->logicalData()[0], 1); // 1+2i == 1+2i
    EXPECT_EQ(r->logicalData()[1], 0); // 3+4i ~= 3+0i
}

TEST_P(SharedOpsTest, ComplexArrayNe)
{
    eval("r = [1+1i, 2+2i] ~= [1+1i, 2+3i];");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->logicalData()[0], 0); // equal
    EXPECT_EQ(r->logicalData()[1], 1); // not equal
}

TEST_P(SharedOpsTest, ComplexScalarVsArray)
{
    eval("r = (1+1i) == [1+1i, 2+2i, 1+1i];");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->numel(), 3u);
    EXPECT_EQ(r->logicalData()[0], 1);
    EXPECT_EQ(r->logicalData()[1], 0);
    EXPECT_EQ(r->logicalData()[2], 1);
}

TEST_P(SharedOpsTest, ComplexLtThrows)
{
    EXPECT_THROW(eval("r = (1+2i) < (3+4i);"), std::runtime_error);
}

TEST_P(SharedOpsTest, ComplexDoubleEq)
{
    // Complex vs double comparison
    eval("r = (3+0i) == 3;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_TRUE(r->toBool());
}

// ============================================================
// String type tests
// ============================================================

TEST_P(SharedOpsTest, StringLiteral)
{
    eval("s = \"hello\";");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "hello");
}

TEST_P(SharedOpsTest, StringClass)
{
    EXPECT_EQ(evalString("class(\"hello\");"), "string");
}

TEST_P(SharedOpsTest, IsStringTrue)
{
    EXPECT_TRUE(evalBool("isstring(\"abc\");"));
}

TEST_P(SharedOpsTest, IsStringFalseOnChar)
{
    EXPECT_FALSE(evalBool("isstring('abc');"));
}

TEST_P(SharedOpsTest, IsCharFalseOnString)
{
    EXPECT_FALSE(evalBool("ischar(\"abc\");"));
}

TEST_P(SharedOpsTest, StringConcat)
{
    eval("s = \"hello\" + \" world\";");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "hello world");
}

TEST_P(SharedOpsTest, StringEqTrue)
{
    EXPECT_TRUE(evalBool("\"abc\" == \"abc\";"));
}

TEST_P(SharedOpsTest, StringEqFalse)
{
    EXPECT_FALSE(evalBool("\"abc\" == \"xyz\";"));
}

TEST_P(SharedOpsTest, StringNeTrue)
{
    EXPECT_TRUE(evalBool("\"abc\" ~= \"xyz\";"));
}

TEST_P(SharedOpsTest, StringLt)
{
    EXPECT_TRUE(evalBool("\"abc\" < \"xyz\";"));
}

TEST_P(SharedOpsTest, StringToChar)
{
    eval("c = char(\"hello\");");
    auto *c = getVarPtr("c");
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->isChar());
    EXPECT_EQ(c->toString(), "hello");
}

TEST_P(SharedOpsTest, CharToString)
{
    eval("s = string('hello');");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "hello");
}

TEST_P(SharedOpsTest, NumToString)
{
    eval("s = string(42);");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "42");
}

TEST_P(SharedOpsTest, Strlength)
{
    EXPECT_DOUBLE_EQ(evalScalar("strlength(\"hello\");"), 5.0);
}

TEST_P(SharedOpsTest, StringHorzcat)
{
    eval("s = [\"abc\", \"def\"];");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->numel(), 2u);
    EXPECT_EQ(s->stringElem(0), "abc");
    EXPECT_EQ(s->stringElem(1), "def");
}

TEST_P(SharedOpsTest, StringContains)
{
    EXPECT_TRUE(evalBool("contains(\"hello world\", \"world\");"));
    EXPECT_FALSE(evalBool("contains(\"hello\", \"xyz\");"));
}

TEST_P(SharedOpsTest, StringStartsWith)
{
    EXPECT_TRUE(evalBool("startsWith(\"hello\", \"hel\");"));
    EXPECT_FALSE(evalBool("startsWith(\"hello\", \"xyz\");"));
}

TEST_P(SharedOpsTest, StringEndsWith)
{
    EXPECT_TRUE(evalBool("endsWith(\"hello\", \"llo\");"));
    EXPECT_FALSE(evalBool("endsWith(\"hello\", \"xyz\");"));
}

TEST_P(SharedOpsTest, Strrep)
{
    eval("s = strrep(\"hello world\", \"world\", \"there\");");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "hello there");
}

TEST_P(SharedOpsTest, StringCompareWithChar)
{
    // string == char should work
    EXPECT_TRUE(evalBool("\"hello\" == 'hello';"));
}

INSTANTIATE_DUAL(SharedOpsTest);