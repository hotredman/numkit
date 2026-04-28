// tests/stdlib/scope_test.cpp — Workspace scope, clear, exist, who/whos, nargout
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace m_test;

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
    Value r = eval("test_uninit();");
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

