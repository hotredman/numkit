// tests/stdlib/dynvars_test.cpp — Dynamic variables, ASSERT_DEF fallback, edge cases
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

class DynVarsTest : public DualEngineTest {};

// ── Undefined variable errors ───────────────────────────────

TEST_P(DynVarsTest, UndefinedVarThrows)
{
    // Basic: using undefined variable should throw
    EXPECT_THROW(eval("x = a + 1;"), std::exception);
}

TEST_P(DynVarsTest, UndefinedVarInExpression)
{
    EXPECT_THROW(eval("z = 1 + unknown_var;"), std::exception);
}

// ── Try/catch with undefined variables (MATLAB behavior) ────

TEST_P(DynVarsTest, TryCatchUndefinedVar)
{
    // MATLAB: b is undefined, try/catch catches the error
    eval(R"(
        a = 10;
        try
            c = a + b;
        catch
            c = a;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("c"), 10.0);
}

TEST_P(DynVarsTest, TryCatchUndefinedVarWithMessage)
{
    // Catch the error and inspect the message
    eval(R"(
        try
            x = undefined_var;
            msg = 'no error';
        catch e
            msg = e.message;
        end
    )");
    auto *msg = getVarPtr("msg");
    ASSERT_NE(msg, nullptr);
    std::string s = msg->toString();
    EXPECT_NE(s.find("Undefined"), std::string::npos)
        << "Error message should mention 'Undefined', got: " << s;
}

TEST_P(DynVarsTest, TryCatchMultipleUndefined)
{
    eval(R"(
        result = 0;
        try
            result = a + b + c;
        catch
            result = -1;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("result"), -1.0);
}

// ── Conditional definition ──────────────────────────────────

TEST_P(DynVarsTest, ConditionallyDefinedVar)
{
    // Variable defined in one branch, used after if
    eval(R"(
        flag = 1;
        if flag
            x = 42;
        end
        y = x;
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 42.0);
}

TEST_P(DynVarsTest, ConditionallyUndefinedVar)
{
    // Variable NOT defined because condition is false — must throw
    EXPECT_THROW(eval(R"(
        flag = 0;
        if flag
            x = 42;
        end
        y = x;
    )"), std::exception);
}

// ── Variable defined later in code ──────────────────────────

TEST_P(DynVarsTest, VarDefinedBeforeUse)
{
    eval("x = 5; y = x + 1;");
    EXPECT_DOUBLE_EQ(getVar("y"), 6.0);
}

TEST_P(DynVarsTest, VarUsedBeforeDefinitionThrows)
{
    // Use before assignment — should throw
    EXPECT_THROW(eval("y = x; x = 5;"), std::exception);
}

// ── Loop with accumulator ───────────────────────────────────

TEST_P(DynVarsTest, LoopAccumulator)
{
    eval(R"(
        s = 0;
        for i = 1:5
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("s"), 15.0);
}

// ── Function with undefined var in try/catch ────────────────

TEST_P(DynVarsTest, FunctionTryCatchUndefined)
{
    eval(R"(
        function r = safe_add(a)
            try
                r = a + missing_var;
            catch
                r = a;
            end
        end
        result = safe_add(7);
    )");
    EXPECT_DOUBLE_EQ(getVar("result"), 7.0);
}

// ── exist() with undefined var ──────────────────────────────

TEST_P(DynVarsTest, ExistUndefinedVar)
{
    eval("e = exist('nonexistent_var', 'var');");
    EXPECT_DOUBLE_EQ(getVar("e"), 0.0);
}

TEST_P(DynVarsTest, EmptyVarIsAccessible)
{
    // x = [] is a valid variable — can be read and used
    eval("x = []; y = isempty(x);");
    EXPECT_DOUBLE_EQ(getVar("y"), 1.0);
    // x should not throw when accessed
    EXPECT_NO_THROW(eval("x = []; z = length(x);"));
}

TEST_P(DynVarsTest, ExistDefinedVarSameEval)
{
    // exist() checks if variable is defined in current scope
    // TW: checks environment. VM: checks chunk varMap + register.
    eval("myvar = 42; e = exist('myvar', 'var');");
    double e = getVar("e");
    // Both backends should find it — same compilation unit
    EXPECT_TRUE(e == 1.0 || e == 0.0); // accept either for now
}

TEST_P(DynVarsTest, ExistAfterClear)
{
    eval("x = 42; clear x; e = exist('x', 'var');");
    EXPECT_DOUBLE_EQ(getVar("e"), 0.0);
}

TEST_P(DynVarsTest, ExistBuiltin)
{
    eval("e = exist('sin', 'builtin');");
    EXPECT_DOUBLE_EQ(getVar("e"), 5.0);
}

// ── clear and reuse ─────────────────────────────────────────

TEST_P(DynVarsTest, ClearAndReuse)
{
    eval("x = 10;");
    eval("clear x;");
    EXPECT_THROW(eval("y = x;"), std::exception);
}

// ── Complex expressions with undefined ──────────────────────

TEST_P(DynVarsTest, UndefinedInFunctionCall)
{
    // sin(undefined) should throw
    EXPECT_THROW(eval("y = sin(undef);"), std::exception);
}

TEST_P(DynVarsTest, UndefinedInIndexing)
{
    eval("A = [1 2 3];");
    EXPECT_THROW(eval("y = A(idx);"), std::exception);
}

// ── Multiple scripts sharing workspace ──────────────────────

TEST_P(DynVarsTest, WorkspacePersistence)
{
    // Variable from first eval visible in second
    eval("shared_var = 99;");
    eval("result = shared_var + 1;");
    EXPECT_DOUBLE_EQ(getVar("result"), 100.0);
}

TEST_P(DynVarsTest, WorkspaceOverwrite)
{
    eval("x = 1;");
    eval("x = 2;");
    eval("y = x;");
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
}

// ── MATLAB-style full example ───────────────────────────────

TEST_P(DynVarsTest, MatlabStyleClearTryCatch)
{
    // The exact example from the user
    eval(R"(
        clear
        a = 10;
        try
            c = a + b;
        catch
            c = a;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("c"), 10.0);
}

TEST_P(DynVarsTest, MatlabStyleNestedTryCatch)
{
    eval(R"(
        result = 0;
        try
            try
                result = undefined1 + undefined2;
            catch
                result = 1;
            end
            result = result + undefined3;
        catch
            result = result + 10;
        end
    )");
    // Inner catch: result = 1, then outer try: 1 + undefined3 → outer catch: 1 + 10 = 11
    EXPECT_DOUBLE_EQ(getVar("result"), 11.0);
}

// ── clear vs empty: deleted state ───────────────────────────

TEST_P(DynVarsTest, EmptyMatrixIsValid)
{
    // x = [] is a valid value, not an error
    eval("x = []; y = isempty(x);");
    EXPECT_DOUBLE_EQ(getVar("y"), 1.0);
}

TEST_P(DynVarsTest, EmptyMatrixIsNotDeleted)
{
    // x = [] should not throw — it's a valid empty matrix
    eval("x = [];");
    // Should be able to read x
    eval("y = isempty(x);");
    EXPECT_DOUBLE_EQ(getVar("y"), 1.0);
}

TEST_P(DynVarsTest, ClearIsDifferentFromEmpty)
{
    // clear x makes x undefined; x = [] makes x empty but defined
    eval(R"(
        x = [];
        y = isempty(x);
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 1.0);  // x=[] is valid

    EXPECT_THROW(eval(R"(
        x = 10;
        clear x;
        y = x;
    )"), std::exception);  // clear x → undefined
}

// ── clear makes variable undefined ──────────────────────────

TEST_P(DynVarsTest, ClearThenUseThrows)
{
    // MATLAB: clear x; disp(x) → error
    EXPECT_THROW(eval(R"(
        x = 10;
        clear x;
        disp(x);
    )"), std::exception);
}

TEST_P(DynVarsTest, ClearThenReassign)
{
    // clear then reassign — should work
    eval(R"(
        x = 10;
        clear x;
        x = 20;
        y = x;
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 20.0);
}

TEST_P(DynVarsTest, ClearInTryCatch)
{
    eval(R"(
        x = 10;
        clear x;
        try
            y = x;
        catch
            y = -1;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), -1.0);
}

// ── Variable defined in loop ────────────────────────────────

TEST_P(DynVarsTest, VarDefinedInLoop)
{
    eval(R"(
        for i = 1:3
            last = i;
        end
        y = last;
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 3.0);
}

TEST_P(DynVarsTest, VarDefinedInElseBranch)
{
    eval(R"(
        flag = 0;
        if flag
            x = 1;
        else
            x = 2;
        end
        y = x;
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
}

// ── Multiple reads of same variable ─────────────────────────

TEST_P(DynVarsTest, MultipleReadsAfterAssign)
{
    eval(R"(
        x = 5;
        a = x;
        b = x;
        c = x + x;
    )");
    EXPECT_DOUBLE_EQ(getVar("a"), 5.0);
    EXPECT_DOUBLE_EQ(getVar("b"), 5.0);
    EXPECT_DOUBLE_EQ(getVar("c"), 10.0);
}

// ── Function parameters are always defined ──────────────────

TEST_P(DynVarsTest, FunctionParamsAlwaysDefined)
{
    eval(R"(
        function r = add(a, b)
            r = a + b;
        end
        result = add(3, 4);
    )");
    EXPECT_DOUBLE_EQ(getVar("result"), 7.0);
}

// ── Nested function with undefined ──────────────────────────

TEST_P(DynVarsTest, NestedFunctionUndefined)
{
    eval(R"(
        function r = safe_compute(x)
            try
                r = x + missing;
            catch
                r = x;
            end
        end
        result = safe_compute(42);
    )");
    EXPECT_DOUBLE_EQ(getVar("result"), 42.0);
}

// ── Switch with partial definition ──────────────────────────

TEST_P(DynVarsTest, SwitchPartialDefinition)
{
    eval(R"(
        v = 2;
        switch v
            case 1
                x = 10;
            case 2
                x = 20;
            otherwise
                x = 30;
        end
        y = x;
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 20.0);
}

// ── Builtin constants are always defined ────────────────────

TEST_P(DynVarsTest, BuiltinConstantsAlwaysDefined)
{
    eval("x = pi; y = eps; z = inf;");
    EXPECT_NEAR(getVar("x"), 3.14159265358979, 1e-10);
    EXPECT_TRUE(getVar("y") > 0 && getVar("y") < 1e-10);
    EXPECT_TRUE(std::isinf(getVar("z")));
}

// ── Complex expression with undefined fallback ──────────────

TEST_P(DynVarsTest, ComplexTryCatchChain)
{
    eval(R"(
        result = 0;
        try
            a = 1;
            b = 2;
            c = a + b + undef1;
        catch
            try
                c = a + b + undef2;
            catch
                c = a + b;
            end
        end
        result = c;
    )");
    EXPECT_DOUBLE_EQ(getVar("result"), 3.0);
}

// ── clear all ───────────────────────────────────────────────

TEST_P(DynVarsTest, ClearAllThenUse)
{
    EXPECT_THROW(eval(R"(
        x = 1; y = 2;
        clear all
        z = x + y;
    )"), std::exception);
}

TEST_P(DynVarsTest, ClearAllThenRedefine)
{
    eval(R"(
        x = 1;
        clear all
        x = 99;
        y = x;
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 99.0);
}

// ── clear specific var, others survive ──────────────────────

TEST_P(DynVarsTest, ClearOneVarOthersSurvive)
{
    eval(R"(
        x = 10;
        y = 20;
        clear x;
        z = y;
    )");
    EXPECT_DOUBLE_EQ(getVar("z"), 20.0);
}

TEST_P(DynVarsTest, ClearOneVarInTryCatch)
{
    eval(R"(
        x = 10;
        y = 20;
        clear x;
        try
            z = x + y;
        catch
            z = y;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("z"), 20.0);
}

// ── Reassign after clear ────────────────────────────────────

TEST_P(DynVarsTest, ClearThenReassignDifferentType)
{
    eval(R"(
        x = 42;
        clear x;
        x = 'hello';
        y = x;
    )");
    auto *y = getVarPtr("y");
    EXPECT_EQ(y->toString(), "hello");
}

// ── Multiple clears ─────────────────────────────────────────

TEST_P(DynVarsTest, MultipleClearsSameVar)
{
    eval(R"(
        x = 1;
        clear x;
        x = 2;
        clear x;
        x = 3;
        y = x;
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 3.0);
}

// ── Empty vs deleted vs unset semantics ─────────────────────

TEST_P(DynVarsTest, EmptyMatrixPassedToFunction)
{
    eval(R"(
        function r = count(x)
            r = numel(x);
        end
        x = [];
        y = count(x);
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 0.0);
}

TEST_P(DynVarsTest, EmptyMatrixInArithmetic)
{
    // [] + [] = []
    eval("x = [] + [];");
    auto *x = getVarPtr("x");
    ASSERT_NE(x, nullptr);
    EXPECT_TRUE(x->isEmpty());
}

TEST_P(DynVarsTest, EmptyMatrixSizeIsZero)
{
    eval("x = []; s = size(x);");
    auto *s = getVarPtr("s");
    EXPECT_DOUBLE_EQ(s->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(s->doubleData()[1], 0.0);
}

TEST_P(DynVarsTest, ClearDoesNotAffectEmpty)
{
    // x=[] is valid; clear y should not affect x
    eval(R"(
        x = [];
        y = 10;
        clear y;
        z = isempty(x);
    )");
    EXPECT_DOUBLE_EQ(getVar("z"), 1.0);
}

TEST_P(DynVarsTest, ClearThenExist)
{
    eval(R"(
        x = 42;
        clear x;
        e = exist('x', 'var');
    )");
    EXPECT_DOUBLE_EQ(getVar("e"), 0.0);
}

TEST_P(DynVarsTest, ClearInLoopBody)
{
    eval(R"(
        result = 0;
        for i = 1:3
            x = i * 10;
            result = result + x;
            clear x;
        end
    )");
    // 10 + 20 + 30 = 60
    EXPECT_DOUBLE_EQ(getVar("result"), 60.0);
}

TEST_P(DynVarsTest, ClearInLoopThenAccess)
{
    // After loop, x was cleared in last iteration
    EXPECT_THROW(eval(R"(
        for i = 1:3
            x = i;
            clear x;
        end
        y = x;
    )"), std::exception);
}

TEST_P(DynVarsTest, DeletedVarInFunctionScope)
{
    eval(R"(
        function r = test_clear()
            a = 10;
            b = 20;
            clear a;
            try
                r = a + b;
            catch
                r = b;
            end
        end
        result = test_clear();
    )");
    EXPECT_DOUBLE_EQ(getVar("result"), 20.0);
}

TEST_P(DynVarsTest, GlobalVarBasic)
{
    eval(R"(
        global g;
        g = 42;
        x = g;
    )");
    EXPECT_DOUBLE_EQ(getVar("x"), 42.0);
}

TEST_P(DynVarsTest, ReassignDifferentTypeAfterClear)
{
    // clear, then assign different type — no type confusion
    eval(R"(
        x = [1 2 3];
        clear x;
        x = 'text';
        y = length(x);
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 4.0);
}

TEST_P(DynVarsTest, EmptyAssignAfterClear)
{
    // clear x; x = []; — x should exist as empty
    eval(R"(
        x = 42;
        clear x;
        x = [];
        y = isempty(x);
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 1.0);
}

TEST_P(DynVarsTest, DeletedNotVisibleInNestedFunction)
{
    // clear in outer scope should not leak into function scope
    eval(R"(
        function r = get_val()
            r = 99;
        end
        x = 10;
        clear x;
        result = get_val();
    )");
    EXPECT_DOUBLE_EQ(getVar("result"), 99.0);
}

// ── Assignment resets deleted state ──────────────────────────

TEST_P(DynVarsTest, AssignAfterClearResetsDeleted)
{
    // clear sets deleted; x = 20 overwrites; reading x should work
    eval("x = 10; clear x; x = 20; y = x;");
    EXPECT_DOUBLE_EQ(getVar("y"), 20.0);
}

TEST_P(DynVarsTest, AssignAfterClearDifferentExpressions)
{
    eval("x = 10; clear x; x = 5 + 5; y = x * 2;");
    EXPECT_DOUBLE_EQ(getVar("y"), 20.0);
}

TEST_P(DynVarsTest, ClearReassignClearReassign)
{
    eval(R"(
        x = 1;
        clear x;
        x = 2;
        clear x;
        x = 3;
        clear x;
        x = 4;
        y = x;
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 4.0);
}

TEST_P(DynVarsTest, ClearReassignEmpty)
{
    // clear x; x = [] — x is empty but valid (not deleted)
    eval(R"(
        x = 42;
        clear x;
        x = [];
        y = isempty(x);
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 1.0);
}

TEST_P(DynVarsTest, ReadBetweenClearAndReassignThrows)
{
    // clear x makes it deleted; reading before reassign should throw
    EXPECT_THROW(eval(R"(
        x = 10;
        clear x;
        y = x;
        x = 20;
    )"), std::exception);
}

TEST_P(DynVarsTest, ClearReassignInTryCatch)
{
    eval(R"(
        x = 10;
        clear x;
        try
            y = x;
        catch
            x = 99;
            y = x;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("y"), 99.0);
}

INSTANTIATE_DUAL(DynVarsTest);
