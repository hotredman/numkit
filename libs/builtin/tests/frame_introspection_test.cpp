// libs/builtin/tests/frame_introspection_test.cpp
//
// assignin / evalin / inputname semantics. These builtins reach into
// the caller's workspace via Engine::callerEnv / callerFrame APIs
// (Phase A). VM mode performs write-through to the caller's frame
// register so subsequent static reads pick up the value.
//
// Parameterized: runs on both TreeWalker and VM backends.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class AssigninTest : public DualEngineTest {};

// ── assignin('base', ...) — writes to top-level workspace ─────

TEST_P(AssigninTest, AssignToBaseFromTopLevel)
{
    eval("assignin('base', 'x', 42);");
    EXPECT_DOUBLE_EQ(getVar("x"), 42.0);
}

TEST_P(AssigninTest, AssignToBaseFromInsideFunction)
{
    eval(R"(
        function helper()
            assignin('base', 'leaked', 99);
        end
    )");
    eval("helper();");
    EXPECT_DOUBLE_EQ(getVar("leaked"), 99.0);
}

TEST_P(AssigninTest, AssignToBaseOverwritesExisting)
{
    eval("x = 1;");
    eval("assignin('base', 'x', 100);");
    EXPECT_DOUBLE_EQ(getVar("x"), 100.0);
}

// ── assignin('caller', ...) — writes to caller's workspace ─────

TEST_P(AssigninTest, AssignToCallerFromSetterFunction)
{
    // setter -> assignin('caller', ...) writes into f's workspace.
    // f reads x via static LOAD; write-through must update the
    // register slot for x in f's frame.
    eval(R"(
        function setter(name, val)
            assignin('caller', name, val);
        end
        function r = f()
            setter('x', 42);
            r = x;
        end
    )");
    eval("y = f();");
    EXPECT_DOUBLE_EQ(getVar("y"), 42.0);
}

TEST_P(AssigninTest, AssignToCallerFromTopLevelThrows)
{
    // 'caller' is invalid in the base workspace (matches MATLAB).
    EXPECT_THROW(eval("assignin('caller', 'x', 5);"), std::exception);
}

TEST_P(AssigninTest, AssignToCallerFromTopLevelFunctionWritesToBase)
{
    // Function called directly from top-level: 'caller' = base.
    eval(R"(
        function leak()
            assignin('caller', 'base_leak', 7);
        end
    )");
    eval("leak();");
    EXPECT_DOUBLE_EQ(getVar("base_leak"), 7.0);
}

TEST_P(AssigninTest, AssignToCallerNestedThreeLevels)
{
    // wrapper -> setter -> assignin('caller'). 'caller' from inside
    // setter is wrapper; assignin must NOT skip past wrapper to f.
    eval(R"(
        function setter(name, val)
            assignin('caller', name, val);
        end
        function wrapper(name, val)
            setter(name, val);
        end
        function r = f()
            wrapper('x', 11);
            % `x` was written into wrapper's scope, not f's.
            % f's `x` is therefore undefined.
            try
                r = x;
            catch
                r = -1;
            end
        end
    )");
    eval("y = f();");
    EXPECT_DOUBLE_EQ(getVar("y"), -1.0);
}

// ── argument validation ─────────────────────────────────────────

TEST_P(AssigninTest, MissingArgsThrows)
{
    EXPECT_THROW(eval("assignin();"), std::exception);
    EXPECT_THROW(eval("assignin('base');"), std::exception);
    EXPECT_THROW(eval("assignin('base', 'x');"), std::exception);
}

TEST_P(AssigninTest, BadWorkspaceNameThrows)
{
    EXPECT_THROW(eval("assignin('global', 'x', 1);"), std::exception);
    EXPECT_THROW(eval("assignin('whatever', 'x', 1);"), std::exception);
}

TEST_P(AssigninTest, NonStringWorkspaceThrows)
{
    EXPECT_THROW(eval("assignin(1, 'x', 1);"), std::exception);
}

TEST_P(AssigninTest, NonStringNameThrows)
{
    EXPECT_THROW(eval("assignin('base', 1, 1);"), std::exception);
}

TEST_P(AssigninTest, EmptyNameThrows)
{
    EXPECT_THROW(eval("assignin('base', '', 1);"), std::exception);
}

INSTANTIATE_DUAL(AssigninTest);

// ============================================================
// evalin / eval / run — caller-scoped re-entrant eval
// ============================================================

class EvalinTest : public DualEngineTest {};

TEST_P(EvalinTest, EvalinBaseFromTopLevel)
{
    eval("evalin('base', 'x = 7;');");
    EXPECT_DOUBLE_EQ(getVar("x"), 7.0);
}

TEST_P(EvalinTest, EvalinBaseFromInsideFunction)
{
    eval(R"(
        function helper()
            evalin('base', 'leaked = 13;');
        end
    )");
    eval("helper();");
    EXPECT_DOUBLE_EQ(getVar("leaked"), 13.0);
}

TEST_P(EvalinTest, EvalinCallerWritesToCallerScope)
{
    // setter does evalin('caller', 'name = val'). 'caller' is f's
    // workspace; f then reads x via static LOAD which must see the
    // assigned value (write-through to f's frame register).
    eval(R"(
        function setter(name, val)
            evalin('caller', [name, ' = ', num2str(val), ';']);
        end
        function r = f()
            setter('x', 99);
            r = x;
        end
    )");
    eval("y = f();");
    EXPECT_DOUBLE_EQ(getVar("y"), 99.0);
}

TEST_P(EvalinTest, EvalinCallerFromTopLevelThrows)
{
    EXPECT_THROW(eval("evalin('caller', 'x = 1;');"), std::exception);
}

TEST_P(EvalinTest, EvalReturnsValue)
{
    // eval('expr') returns the expression's value.
    EXPECT_DOUBLE_EQ(evalScalar("eval('1 + 2 + 3');"), 6.0);
}

TEST_P(EvalinTest, EvalDefinesVariableInCallerScope)
{
    // eval('x = 5') from inside a function defines x in that
    // function's scope. After return, x is gone from the function;
    // top-level workspace shouldn't see it either (it was scoped to f).
    eval(R"(
        function r = f()
            eval('x = 5;');
            r = x;
        end
    )");
    eval("y = f();");
    EXPECT_DOUBLE_EQ(getVar("y"), 5.0);
}

TEST_P(EvalinTest, EvalAtTopLevelPersists)
{
    // eval at top-level == REPL semantics; vars persist in workspace.
    eval("eval('persist_v = 42;');");
    EXPECT_DOUBLE_EQ(getVar("persist_v"), 42.0);
}

TEST_P(EvalinTest, EvalImportInsideFunctionStaysLocal)
{
    // Critical case: eval('import x.*') inside a function must scope
    // the import to the function's frame. After function returns,
    // workspace must NOT see the import.
    engine.registerFunction(
        "myns_for_evalin", "v",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = Value::scalar(101.0, ctx.engine->resource());
        });
    eval(R"(
        function r = f()
            eval('import myns_for_evalin.*;');
            r = v();
        end
    )");
    eval("y = f();");
    EXPECT_DOUBLE_EQ(getVar("y"), 101.0);
    // Top-level call to v() must fail (import was function-local).
    EXPECT_THROW(eval("z = v();"), std::exception);
}

TEST_P(EvalinTest, EvalinBadWorkspaceThrows)
{
    EXPECT_THROW(eval("evalin('global', 'x = 1');"), std::exception);
}

TEST_P(EvalinTest, EvalinMissingArgsThrows)
{
    EXPECT_THROW(eval("evalin();"), std::exception);
    EXPECT_THROW(eval("evalin('base');"), std::exception);
}

INSTANTIATE_DUAL(EvalinTest);
