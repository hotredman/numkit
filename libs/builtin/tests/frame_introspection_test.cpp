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

// ============================================================
// inputname — caller's arg names from call site
// ============================================================

class InputnameTest : public DualEngineTest {};

TEST_P(InputnameTest, BareIdentifierArgReturnsName)
{
    eval(R"(
        function r = get_name(x)
            r = inputname(1);
        end
    )");
    eval("myvar = 99;");
    eval("n = get_name(myvar);");
    EXPECT_EQ(getVarPtr("n")->toString(), "myvar");
}

TEST_P(InputnameTest, LiteralArgReturnsEmpty)
{
    eval(R"(
        function r = get_name(x)
            r = inputname(1);
        end
    )");
    eval("n = get_name(42);");
    EXPECT_EQ(getVarPtr("n")->toString(), "");
}

TEST_P(InputnameTest, ExpressionArgReturnsEmpty)
{
    eval(R"(
        function r = get_name(x)
            r = inputname(1);
        end
    )");
    eval("a = 1; b = 2; n = get_name(a + b);");
    EXPECT_EQ(getVarPtr("n")->toString(), "");
}

TEST_P(InputnameTest, MultipleArgsMixedTypes)
{
    eval(R"(
        function [n1, n2, n3] = get_names(a, b, c)
            n1 = inputname(1);
            n2 = inputname(2);
            n3 = inputname(3);
        end
    )");
    eval("foo = 1; bar = 2; [r1, r2, r3] = get_names(foo, 5, bar);");
    EXPECT_EQ(getVarPtr("r1")->toString(), "foo");
    EXPECT_EQ(getVarPtr("r2")->toString(), "");
    EXPECT_EQ(getVarPtr("r3")->toString(), "bar");
}

TEST_P(InputnameTest, OutsideFunctionThrows)
{
    EXPECT_THROW(eval("n = inputname(1);"), std::exception);
}

TEST_P(InputnameTest, IndexLessThanOneThrows)
{
    eval(R"(
        function r = get(x)
            r = inputname(0);
        end
    )");
    EXPECT_THROW(eval("get(42);"), std::exception);
}

TEST_P(InputnameTest, IndexBeyondArgsReturnsEmpty)
{
    // MATLAB: inputname(k) for k > nargin returns empty rather than
    // throwing. Our implementation matches this when the call site
    // metadata is shorter than k.
    eval(R"(
        function r = get(x)
            r = inputname(2);
        end
    )");
    eval("n = get(99);");
    EXPECT_EQ(getVarPtr("n")->toString(), "");
}

INSTANTIATE_DUAL(InputnameTest);

// ============================================================
// Phase B+ — read-side evalin (caller-frame var pre-population)
// ============================================================

class EvalinReadSideTest : public DualEngineTest {};

TEST_P(EvalinReadSideTest, EvalinReadsCallerLocalVar)
{
    // Inner script reads x which lives only in caller's R[reg].
    // Pre-populated dynVars overlay must surface it.
    eval(R"(
        function r = peek()
            r = evalin('caller', 'x + 1');
        end
        function r = f()
            x = 5;
            r = peek();
        end
    )");
    eval("y = f();");
    EXPECT_DOUBLE_EQ(getVar("y"), 6.0);
}

TEST_P(EvalinReadSideTest, EvalinReadsAndWritesCallerLocal)
{
    // Both read and update; write-through lets caller's static read
    // observe the new value.
    eval(R"(
        function bump()
            evalin('caller', 'x = x + 10;');
        end
        function r = f()
            x = 5;
            bump();
            r = x;
        end
    )");
    eval("y = f();");
    EXPECT_DOUBLE_EQ(getVar("y"), 15.0);
}

TEST_P(EvalinReadSideTest, EvalReadsCallerLocalThroughDynamicLookup)
{
    // assignin sets x which caller never references statically.
    // Caller's dynamic `eval(['disp(' n ')'])` must find x via
    // dynVars-from-snapshot.
    eval(R"(
        function setit(name, val)
            assignin('caller', name, val);
        end
        function r = f()
            setit('z', 77);
            n = 'z';
            r = eval(n);
        end
    )");
    eval("y = f();");
    EXPECT_DOUBLE_EQ(getVar("y"), 77.0);
}

INSTANTIATE_DUAL(EvalinReadSideTest);

// ============================================================
// Phase B+ — function-handle inputname (CALL_INDIRECT path)
// ============================================================

class InputnameHandleTest : public DualEngineTest {};

TEST_P(InputnameHandleTest, InputnameThroughFunctionHandle)
{
    eval(R"(
        function r = grab(x)
            r = inputname(1);
        end
    )");
    eval("h = @grab; vname = 88; n = h(vname);");
    EXPECT_EQ(getVarPtr("n")->toString(), "vname");
}

INSTANTIATE_DUAL(InputnameHandleTest);

// ============================================================
// Phase B+ — previously untested edge cases (#5, #7, #8 from
// the limitations table)
// ============================================================

class FrameIntrospectionEdgesTest : public DualEngineTest {};

// #5: re-entrant eval depth >= 3
TEST_P(FrameIntrospectionEdgesTest, ReentrantEvalDepth3)
{
    // Three levels of re-entrant eval. Each layer halves the
    // doubled-single-quote count: '''' -> '' -> ' (MATLAB string
    // escape rule). inheritedScope_ save/restore via C++ stack must
    // handle this without crosstalk.
    eval("eval('eval(''eval(''''deep_v = 42;'''')'')');");
    EXPECT_DOUBLE_EQ(getVar("deep_v"), 42.0);
}

// #8: global x inside evalin
TEST_P(FrameIntrospectionEdgesTest, GlobalInsideEvalinBase)
{
    eval(R"(
        evalin('base', 'global gv; gv = 314;');
    )");
    eval("global gv;");
    EXPECT_DOUBLE_EQ(getVar("gv"), 314.0);
}

// #7-style: nargin inside eval'd code matches caller's nargin
TEST_P(FrameIntrospectionEdgesTest, NarginInsideEvalReflectsEnclosingFunction)
{
    // Inside f(a, b), eval('nargin') should read f's nargin (= 2).
    // inheritedScope_ routes the inner script's lookups through f's
    // frame.env. nargin is in the env (set on entry).
    eval(R"(
        function r = f(a, b)
            r = eval('nargin');
        end
    )");
    eval("y = f(10, 20);");
    EXPECT_DOUBLE_EQ(getVar("y"), 2.0);
}

INSTANTIATE_DUAL(FrameIntrospectionEdgesTest);

// ============================================================
// Sanity: core builtins available WITHOUT `import compat.*`
//
// The DualEngineTest fixture imports compat in SetUp for ergonomics,
// but core workspace builtins (clear, who, whos, cd, pwd, eval, run,
// assignin, evalin, inputname, import itself) are registered via the
// 1-arg form of `registerFunction(name, fn)` — they live directly in
// the externalFuncs_ map under their bare leaf name, NOT inside any
// namespace. findExternal does a direct map hit before walking
// imports. Verify on a pristine engine without compat.
// ============================================================

#include <gtest/gtest.h>
#include <numkit/core/engine.hpp>

namespace {

class CoreBuiltinsNoCompatTest : public ::testing::TestWithParam<numkit::Engine::Backend> {};

TEST_P(CoreBuiltinsNoCompatTest, ClearWorksWithoutImport)
{
    numkit::Engine e;
    e.setBackend(GetParam());
    // NO `import compat.*` here.
    e.eval("x = 42;");
    ASSERT_NE(e.getVariable("x"), nullptr);
    EXPECT_NO_THROW(e.eval("clear x;"));
    EXPECT_EQ(e.getVariable("x"), nullptr);
}

TEST_P(CoreBuiltinsNoCompatTest, ClearAllWorksWithoutImport)
{
    numkit::Engine e;
    e.setBackend(GetParam());
    e.eval("a=1; b=2; c=3;");
    EXPECT_NO_THROW(e.eval("clear all;"));
    EXPECT_EQ(e.getVariable("a"), nullptr);
    EXPECT_EQ(e.getVariable("b"), nullptr);
    EXPECT_EQ(e.getVariable("c"), nullptr);
}

TEST_P(CoreBuiltinsNoCompatTest, WhoWhosWorkWithoutImport)
{
    numkit::Engine e;
    e.setBackend(GetParam());
    e.eval("v = 5;");
    EXPECT_NO_THROW(e.eval("who;"));
    EXPECT_NO_THROW(e.eval("whos;"));
}

TEST_P(CoreBuiltinsNoCompatTest, EvalWorksWithoutImport)
{
    numkit::Engine e;
    e.setBackend(GetParam());
    EXPECT_NO_THROW(e.eval("eval('q = 7;');"));
    auto *q = e.getVariable("q");
    ASSERT_NE(q, nullptr);
    EXPECT_DOUBLE_EQ(q->toScalar(), 7.0);
}

TEST_P(CoreBuiltinsNoCompatTest, AssigninEvalinWorkWithoutImport)
{
    numkit::Engine e;
    e.setBackend(GetParam());
    EXPECT_NO_THROW(e.eval("assignin('base', 'foo', 99);"));
    auto *foo = e.getVariable("foo");
    ASSERT_NE(foo, nullptr);
    EXPECT_DOUBLE_EQ(foo->toScalar(), 99.0);

    EXPECT_NO_THROW(e.eval("evalin('base', 'bar = 11;');"));
    auto *bar = e.getVariable("bar");
    ASSERT_NE(bar, nullptr);
    EXPECT_DOUBLE_EQ(bar->toScalar(), 11.0);
}

TEST_P(CoreBuiltinsNoCompatTest, ImportItselfWorksWithoutCompat)
{
    numkit::Engine e;
    e.setBackend(GetParam());
    // `import` is a builtin too — must be available without import.
    EXPECT_NO_THROW(e.eval("import('signal.windows.*');"));
}

// Graphics promotions: figure / close / hold are workspace-style
// session commands and live in core (triple-registered alongside
// graphics.layout.<name> + compat.<name>). Pin that they work
// without `import compat.*` or `import graphics.*`.
TEST_P(CoreBuiltinsNoCompatTest, FigureClosePromotedToCore)
{
    numkit::Engine e;
    e.setBackend(GetParam());
    EXPECT_NO_THROW(e.eval("figure(1);"));
    EXPECT_NO_THROW(e.eval("figure(2);"));
    EXPECT_NO_THROW(e.eval("close;"));
    EXPECT_NO_THROW(e.eval("close all;"));
}

TEST_P(CoreBuiltinsNoCompatTest, HoldPromotedToCore)
{
    numkit::Engine e;
    e.setBackend(GetParam());
    EXPECT_NO_THROW(e.eval("figure(1);"));
    EXPECT_NO_THROW(e.eval("hold on;"));
    EXPECT_NO_THROW(e.eval("hold off;"));
}

INSTANTIATE_TEST_SUITE_P(TW_VM, CoreBuiltinsNoCompatTest,
    ::testing::Values(numkit::Engine::Backend::TreeWalker,
                      numkit::Engine::Backend::VM),
    [](const ::testing::TestParamInfo<numkit::Engine::Backend> &info) {
        return info.param == numkit::Engine::Backend::TreeWalker ? "TW" : "VM";
    });

} // namespace
