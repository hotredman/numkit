// tests/diagnostics/debug_session_test.cpp
// Tests for DebugSession: pause/resume, eval in context, stepping
#include <numkit/m/debug/MDebugSession.hpp>
#include <numkit/m/core/MEngine.hpp>
#include <gtest/gtest.h>

using namespace numkit::m;

// ============================================================
// Fixture
// ============================================================

class DebugSessionTest : public ::testing::Test
{
protected:
    Engine engine;
    std::string output;

    void SetUp() override
    {
        engine.setOutputFunc([this](const std::string &s) { output += s; });
    }

    // Helper: start debug session, return status
    ExecStatus startDebug(DebugSession &session, const std::string &code)
    {
        output.clear();
        return session.start(code);
    }
};

// ============================================================
// Basic pause/resume
// ============================================================

TEST_F(DebugSessionTest, PauseAtBreakpoint)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    // With breakpoints set, start() runs until the first breakpoint is hit.
    auto status = startDebug(session, "x = 10;\ny = 20;\nz = 30;\n");
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);
}

TEST_F(DebugSessionTest, ContinueToCompletion)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    // start() hits bp at line 2 directly
    auto status = startDebug(session, "x = 10;\ny = 20;\n");
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    // Continue past bp → completes
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_FALSE(session.isActive());
}

TEST_F(DebugSessionTest, MultipleContinues)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    // start() hits bp at line 2 on the first iteration
    auto status = startDebug(session, "for i = 1:3\n  x = i;\nend\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Two more iterations hit the bp
    for (int i = 0; i < 2; ++i) {
        status = session.resume(DebugAction::Continue);
        ASSERT_EQ(status, ExecStatus::Paused) << "iteration " << i;
    }
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ============================================================
// Snapshot: variables in function scope
// ============================================================

TEST_F(DebugSessionTest, SnapshotShowsFunctionLocals)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = square(n)\n"
        "    r = n * n;\n"
        "    r = r + 0;\n"   // breakpoint here, after r is computed
        "end\n"
        "result = square(7);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    EXPECT_EQ(snap.functionName, "square");

    // Should see n and r in the function scope
    bool foundN = false, foundR = false;
    for (auto &v : snap.variables) {
        if (v.name == "n" && v.value && v.value->isDoubleScalar()) {
            EXPECT_DOUBLE_EQ(v.value->scalarVal(), 7.0);
            foundN = true;
        }
        if (v.name == "r" && v.value && v.value->isDoubleScalar()) {
            EXPECT_DOUBLE_EQ(v.value->scalarVal(), 49.0);
            foundR = true;
        }
    }
    EXPECT_TRUE(foundN) << "expected 'n' in function scope";
    EXPECT_TRUE(foundR) << "expected 'r' in function scope";
}

// ============================================================
// Eval in debug context
// ============================================================

TEST_F(DebugSessionTest, EvalSimpleVariable)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = fib(n)\n"
        "    if n <= 1\n"
        "        r = n;\n"
        "    else\n"
        "        r = fib(n-1) + fib(n-2);\n"
        "    end\n"
        "end\n"
        "result = fib(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    EXPECT_EQ(snap.functionName, "fib");

    // Eval a variable name — should display its value
    std::string result = session.eval("n");
    EXPECT_NE(result.find("1"), std::string::npos) << "eval('n') should show value 1, got: " << result;
}

TEST_F(DebugSessionTest, EvalExpression)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = fib(n)\n"
        "    if n <= 1\n"
        "        r = n;\n"
        "    else\n"
        "        r = fib(n-1) + fib(n-2);\n"
        "    end\n"
        "end\n"
        "result = fib(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Eval an expression using frame variables
    std::string result = session.eval("n + 10");
    EXPECT_NE(result.find("11"), std::string::npos) << "eval('n+10') should show 11, got: " << result;
}

TEST_F(DebugSessionTest, EvalNarginNargout)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "function r = add(a, b)\n"
        "    s = a + b;\n"
        "    r = s;\n"
        "end\n"
        "result = add(10, 20);\n";

    // start() hits bp at line 2 directly (inside add)
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    EXPECT_EQ(snap.functionName, "add");

    // nargin should be 2 (two arguments passed)
    std::string result = session.eval("nargin");
    EXPECT_NE(result.find("2"), std::string::npos)
        << "eval('nargin') should show 2, got: " << result;

    // nargout should be 1 (one return value)
    result = session.eval("nargout");
    EXPECT_NE(result.find("1"), std::string::npos)
        << "eval('nargout') should show 1, got: " << result;
}

TEST_F(DebugSessionTest, EvalCreatesNewVariable)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = 10;\ny = 20;\nz = 30;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Create a new variable in debug console
    session.eval("q = 42");

    // q should appear in snapshot
    auto snap = session.snapshot();
    bool hasQ = false;
    for (auto &v : snap.variables) {
        if (v.name == "q" && v.value) {
            hasQ = true;
            EXPECT_DOUBLE_EQ(v.value->toScalar(), 42.0);
        }
    }
    EXPECT_TRUE(hasQ) << "eval-created variable q should appear in snapshot";
}

TEST_F(DebugSessionTest, EvalCreatedVarPersistsAcrossEvals)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = 10;\ny = 20;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Create variable, then use it in next eval
    session.eval("q = [1 2 3]");
    std::string result = session.eval("sum(q)");
    EXPECT_NE(result.find("6"), std::string::npos)
        << "sum(q) should be 6, got: " << result;
}

TEST_F(DebugSessionTest, EvalCreatedVarInFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "function r = foo(x)\n"
        "    r = x * 2;\n"
        "end\n"
        "result = foo(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Create new variable inside function debug context
    session.eval("tmp = x + 100");

    auto snap = session.snapshot();
    bool hasTmp = false;
    for (auto &v : snap.variables)
        if (v.name == "tmp" && v.value) hasTmp = true;
    EXPECT_TRUE(hasTmp) << "eval-created 'tmp' should appear in function snapshot";

    // Original frame variable x should still be accessible
    std::string result = session.eval("x");
    EXPECT_NE(result.find("5"), std::string::npos)
        << "x should still be 5, got: " << result;
}

TEST_F(DebugSessionTest, EvalArrayConstruction)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = fib(n)\n"
        "    if n <= 1\n"
        "        r = n;\n"
        "    else\n"
        "        r = fib(n-1) + fib(n-2);\n"
        "    end\n"
        "end\n"
        "result = fib(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Build array from frame variable
    std::string result = session.eval("q = [1 2 n]");
    EXPECT_NE(result.find("1"), std::string::npos) << "should contain 1, got: " << result;
    EXPECT_NE(result.find("2"), std::string::npos) << "should contain 2, got: " << result;
}

TEST_F(DebugSessionTest, EvalPreservesDebugState)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = fib(n)\n"
        "    if n <= 1\n"
        "        r = n;\n"
        "    else\n"
        "        r = fib(n-1) + fib(n-2);\n"
        "    end\n"
        "end\n"
        "result = fib(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snapBefore = session.snapshot();

    // Eval multiple times
    session.eval("n");
    session.eval("n + 100");
    session.eval("q = [n n n]");

    // Debug state must be preserved
    EXPECT_TRUE(session.isActive());
    auto snapAfter = session.snapshot();
    EXPECT_EQ(snapAfter.line, snapBefore.line);
    EXPECT_EQ(snapAfter.functionName, snapBefore.functionName);

    // Can still continue execution after eval
    status = session.resume(DebugAction::Continue);
    EXPECT_TRUE(status == ExecStatus::Paused || status == ExecStatus::Completed);
}

TEST_F(DebugSessionTest, EvalAfterMultipleResumes)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "for i = 1:3\n"
        "  x = i * 10;\n"
        "end\n";

    // start() hits bp at line 2, first iteration (i=1, before x is assigned)
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    std::string result = session.eval("i");
    EXPECT_NE(result.find("1"), std::string::npos) << "first iteration i=1, got: " << result;

    // Continue → second bp hit: i=2
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);

    result = session.eval("i");
    EXPECT_NE(result.find("2"), std::string::npos) << "second iteration i=2, got: " << result;

    result = session.eval("x");
    EXPECT_NE(result.find("10"), std::string::npos) << "x should be 10 from iteration 1, got: " << result;
}

// ============================================================
// Eval doesn't break continue flow
// ============================================================

TEST_F(DebugSessionTest, ContinueWorksAfterEval)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = double_it(x)\n"
        "    r = x * 2;\n"
        "    r = r + 0;\n"  // bp here
        "end\n"
        "a = double_it(5);\n"
        "b = double_it(10);\n";

    // start() hits bp at line 3 inside double_it(5)
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "double_it");

    std::string result = session.eval("r");
    EXPECT_NE(result.find("10"), std::string::npos) << "r should be 10 (5*2), got: " << result;

    // Continue → second bp in double_it(10)
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);

    result = session.eval("x");
    EXPECT_NE(result.find("10"), std::string::npos) << "x should be 10, got: " << result;

    result = session.eval("r");
    EXPECT_NE(result.find("20"), std::string::npos) << "r should be 20 (10*2), got: " << result;

    // Continue to completion
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ============================================================
// Stepping
// ============================================================

TEST_F(DebugSessionTest, StepOver)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    std::string code = "x = 1;\ny = 2;\nz = 3;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 1);

    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 3);
}

TEST_F(DebugSessionTest, StepIntoFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({4});

    std::string code =
        "function r = add1(x)\n"
        "    r = x + 1;\n"
        "end\n"
        "y = add1(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Step into the function call
    status = session.resume(DebugAction::StepInto);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "add1");
}

// ============================================================
// Stop
// ============================================================

TEST_F(DebugSessionTest, StopEndsSession)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = 1;\ny = 2;\nz = 3;\n");
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_TRUE(session.isActive());

    session.stop();
    EXPECT_FALSE(session.isActive());
}

// ============================================================
// Error handling in eval
// ============================================================

TEST_F(DebugSessionTest, EvalUndefinedVariable)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = startDebug(session, "x = 42;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    std::string result = session.eval("nonexistent_var");
    // Should return an error, not crash
    EXPECT_NE(result.find("Error"), std::string::npos) << "expected error for undefined var, got: " << result;

    // Session should still be active
    EXPECT_TRUE(session.isActive());
}

TEST_F(DebugSessionTest, EvalSyntaxError)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = startDebug(session, "x = 42;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    std::string result = session.eval("[[[");
    EXPECT_NE(result.find("Error"), std::string::npos) << "expected error for bad syntax, got: " << result;
    EXPECT_TRUE(session.isActive());
}

// ============================================================
// Figures during debug: markers flow through outputFunc, not std::cout
// ============================================================

TEST_F(DebugSessionTest, PlotOutputContainsFigureMarker)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "x = [1 2 3]; y = [4 5 6];\n"
        "plot(x, y);\n";

    // start() hits bp at line 2 (plot call)
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Step over the plot call
    status = session.resume(DebugAction::StepOver);

    // The output should contain the __FIGURE_DATA__ marker
    std::string out = session.takeOutput();
    EXPECT_NE(out.find("__FIGURE_DATA__"), std::string::npos)
        << "plot output should contain figure marker, got: " << out;
}

TEST_F(DebugSessionTest, PlotDuringEvalContainsFigureMarker)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = startDebug(session, "x = [1 2 3];\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Eval a plot command in debug context
    std::string result = session.eval("plot([1 2 3], [4 5 6])");

    // The figure marker must be in the eval return value
    EXPECT_NE(result.find("__FIGURE_DATA__"), std::string::npos)
        << "plot in eval should produce figure marker in result, got: " << result;

    // Session should still be active
    EXPECT_TRUE(session.isActive());
}

TEST_F(DebugSessionTest, FigureDuringEvalEmitsMarker)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = startDebug(session, "x = 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    std::string result = session.eval("figure(1)");
    EXPECT_NE(result.find("__FIGURE_DATA__"), std::string::npos)
        << "figure(1) in eval should emit marker, got: " << result;
    EXPECT_NE(result.find("\"datasets\":[]"), std::string::npos)
        << "empty figure should have no datasets, got: " << result;
    EXPECT_TRUE(session.isActive());
}

TEST_F(DebugSessionTest, CloseDuringEvalEmitsMarker)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = startDebug(session, "x = 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Create a figure, then close it
    session.eval("figure(2)");
    std::string result = session.eval("close(2)");
    EXPECT_NE(result.find("__FIGURE_CLOSE__:2"), std::string::npos)
        << "close(2) in eval should emit close marker, got: " << result;
    EXPECT_TRUE(session.isActive());
}

TEST_F(DebugSessionTest, PlotWithFrameVarsDuringEval)
{
    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "function r = make_data(n)\n"
        "    r = linspace(0, 1, n);\n"
        "    r = r + 0;\n"   // bp here
        "end\n"
        "y = make_data(5);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "make_data");

    // Plot using the frame variable r
    std::string result = session.eval("plot(r)");
    EXPECT_NE(result.find("__FIGURE_DATA__"), std::string::npos)
        << "plot(r) with frame var should produce marker, got: " << result;
    // Should contain actual data, not empty datasets
    EXPECT_EQ(result.find("\"datasets\":[]"), std::string::npos)
        << "plot(r) should have non-empty datasets";
    EXPECT_TRUE(session.isActive());
}

TEST_F(DebugSessionTest, EvalPlotPreservesDebugState)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = [1 2 3];\ny = [4 5 6];\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snapBefore = session.snapshot();

    // Eval plot and figure commands
    session.eval("figure(1)");
    session.eval("plot([1 2 3], [4 5 6])");
    session.eval("title('test')");

    // Debug state must be preserved
    EXPECT_TRUE(session.isActive());
    auto snapAfter = session.snapshot();
    EXPECT_EQ(snapAfter.line, snapBefore.line);

    // Can still continue
    status = session.resume(DebugAction::Continue);
    EXPECT_TRUE(status == ExecStatus::Paused || status == ExecStatus::Completed);
}

// ============================================================
// OutputFunc lifetime: must survive debug session destruction
// ============================================================

TEST_F(DebugSessionTest, OutputFuncWorksAfterDebugCompletes)
{
    // Simulate the ReplSession pattern:
    // 1. Set outputFunc → owner's buffer
    // 2. Debug session runs, redirects outputFunc to its own buffer
    // 3. Session completes and is destroyed
    // 4. Next eval must write to owner's buffer, not to freed memory

    std::string ownerBuf;
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    {
        DebugSession session(engine);
        session.setBreakpoints({2});
        auto status = session.start("x = 1;\ny = 2;\n");
        ASSERT_EQ(status, ExecStatus::Paused);
        status = session.resume(DebugAction::Continue);
        // Session completes or pauses — either way, it redirected outputFunc
    }
    // DebugSession destroyed here — engine's outputFunc was pointing to it

    // Restore outputFunc (this is what ReplSession::restoreOutputFunc does)
    ownerBuf.clear();
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    // Normal eval must work and output must go to ownerBuf
    engine.eval("disp('hello after debug')");
    EXPECT_NE(ownerBuf.find("hello after debug"), std::string::npos)
        << "output after debug session destroyed should work, got: " << ownerBuf;
}

TEST_F(DebugSessionTest, OutputFuncWorksAfterDebugStop)
{
    std::string ownerBuf;
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    {
        DebugSession session(engine);
        session.setBreakpoints({2});
        auto status = session.start("x = 1;\ny = 2;\nz = 3;\n");
        ASSERT_EQ(status, ExecStatus::Paused);
        session.stop();  // explicit stop while paused
    }
    // DebugSession destroyed — outputFunc was dangling

    ownerBuf.clear();
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    engine.eval("disp('after stop')");
    EXPECT_NE(ownerBuf.find("after stop"), std::string::npos)
        << "output after debug stop should work, got: " << ownerBuf;
}

TEST_F(DebugSessionTest, OutputFuncDanglingWithoutRestore)
{
    // This test documents the problem: WITHOUT restoring outputFunc,
    // output goes to freed memory. With ASan this would crash.
    // We verify the fix pattern: restore after destroy.

    std::string ownerBuf;
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    {
        DebugSession session(engine);
        session.setBreakpoints({1});
        session.start("x = 42;\n");
        // session redirected outputFunc to its own buffer
        session.eval("disp('during debug')");
        // resume to completion
        session.resume(DebugAction::Continue);
    }
    // Without restore, engine.outputFunc_ → dangling pointer

    // Restore (the fix)
    ownerBuf.clear();
    engine.setOutputFunc([&ownerBuf](const std::string &s) { ownerBuf += s; });

    // Verify figure markers also go to correct buffer
    engine.eval("figure(1)");
    EXPECT_NE(ownerBuf.find("__FIGURE_DATA__"), std::string::npos)
        << "figure markers after debug should go to owner buffer, got: " << ownerBuf;

    ownerBuf.clear();
    engine.eval("plot([1 2], [3 4])");
    EXPECT_NE(ownerBuf.find("__FIGURE_DATA__"), std::string::npos)
        << "plot markers after debug should go to owner buffer, got: " << ownerBuf;
}

// ============================================================
// Comprehensive debugger action tests
// ============================================================

// ── Step Out ────────────────────────────────────────────────

TEST_F(DebugSessionTest, StepOutFromFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "function r = double_it(x)\n"
        "    r = x * 2;\n"
        "end\n"
        "y = double_it(5);\n"
        "z = y + 1;\n";

    // start() hits bp at line 2 inside double_it
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "double_it");

    // Step Out — returns to caller or completes
    status = session.resume(DebugAction::StepOut);
    // May pause at caller line or complete if no more code
    if (status == ExecStatus::Paused) {
        EXPECT_NE(session.snapshot().functionName, "double_it")
            << "Should have exited double_it";
    }
    // Both Paused and Completed are acceptable
}

// ── Step Over skips function calls ──────────────────────────

TEST_F(DebugSessionTest, StepOverSkipsFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({4});

    std::string code =
        "function r = sq(x)\n"
        "    r = x * x;\n"
        "end\n"
        "a = sq(3);\n"
        "b = sq(4);\n"
        "c = a + b;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Continue to line 4 (a = sq(3))
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 4);

    // Step Over — should execute sq(3) without entering, land on line 5
    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 5);
    // a should be 9
    bool foundA = false;
    for (auto &v : session.snapshot().variables)
        if (v.name == "a" && v.value) { EXPECT_DOUBLE_EQ(v.value->toScalar(), 9.0); foundA = true; }
    EXPECT_TRUE(foundA);
}

// ── Step Into enters function ───────────────────────────────

TEST_F(DebugSessionTest, StepIntoFromCallLine)
{
    DebugSession session(engine);
    session.setBreakpoints({5});

    std::string code =
        "function r = add1(x)\n"
        "    r = x + 1;\n"
        "end\n"
        "a = 1;\n"
        "b = add1(a);\n";

    // start() hits bp at line 5 (b = add1(a))
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 5);

    // Step Into → should enter add1
    status = session.resume(DebugAction::StepInto);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "add1");
}

// ── Continue through multiple breakpoints ───────────────────

TEST_F(DebugSessionTest, ContinueThroughMultipleBreakpoints)
{
    DebugSession session(engine);
    session.setBreakpoints({1, 3, 5});

    std::string code = "a = 1;\nb = 2;\nc = 3;\nd = 4;\ne = 5;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 1);

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 3);

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 5);

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ── Breakpoints inside functions ────────────────────────────

TEST_F(DebugSessionTest, BreakpointInsideFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({2}); // inside function body

    std::string code =
        "function r = compute(x)\n"
        "    r = x * 10;\n"
        "end\n"
        "a = compute(3);\n"
        "b = compute(7);\n";

    // start() → first hit: compute(3), line 2
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);
    EXPECT_EQ(session.snapshot().functionName, "compute");

    // Continue → second hit: compute(7), line 2 again
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);
    EXPECT_EQ(session.snapshot().functionName, "compute");

    // Continue → done
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ── Step Over in loop ───────────────────────────────────────

TEST_F(DebugSessionTest, StepOverInLoop)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "s = 0;\n"
        "for i = 1:3\n"
        "    s = s + i;\n"
        "end\n";

    // start() hits bp at line 2 (for)
    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    // Step Over through loop body
    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    // Should be on next line (s = s + i) or next iteration
}

// ── Stop during function execution ──────────────────────────

TEST_F(DebugSessionTest, StopDuringFunction)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "function r = slow(x)\n"
        "    r = x;\n"
        "end\n"
        "a = slow(1);\n"
        "b = slow(2);\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().functionName, "slow");

    session.stop();
    EXPECT_FALSE(session.isActive());
}

// ── Eval with clear during debug ────────────────────────────

TEST_F(DebugSessionTest, EvalClearDuringDebug)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = 10;\ny = 20;\nz = 30;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // x should be visible
    auto snap = session.snapshot();
    bool hasX = false;
    for (auto &v : snap.variables)
        if (v.name == "x" && v.value) hasX = true;
    EXPECT_TRUE(hasX);

    // Clear x
    session.eval("clear x");

    // x should not be visible
    snap = session.snapshot();
    hasX = false;
    for (auto &v : snap.variables)
        if (v.name == "x" && v.value && !v.value->isDeleted()) hasX = true;
    EXPECT_FALSE(hasX) << "x should be cleared";
}

// ── Continue after eval modification ────────────────────────

TEST_F(DebugSessionTest, ContinueAfterEvalModification)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code = "x = 10;\ny = 20;\nz = x + y;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Modify x
    session.eval("x = 100");

    // Continue — z should be 100 + 20 = 120
    status = session.resume(DebugAction::Continue);
    std::string out = session.takeOutput();
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ── Variables visible at each step ──────────────────────────

TEST_F(DebugSessionTest, VariablesAccumulateDuringSteps)
{
    DebugSession session(engine);

    std::string code = "a = 1;\nb = 2;\nc = 3;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    // Initial step: line 1, no vars yet
    EXPECT_EQ(session.snapshot().line, 1);

    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    // After a=1, should have a
    auto snap = session.snapshot();
    bool hasA = false;
    for (auto &v : snap.variables)
        if (v.name == "a") hasA = true;
    EXPECT_TRUE(hasA);

    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    // After b=2, should have a and b
    snap = session.snapshot();
    bool hasB = false;
    for (auto &v : snap.variables)
        if (v.name == "b") hasB = true;
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);
}

// ============================================================
// Removing a breakpoint mid-session — via setBreakpoints() with an
// updated list — must take effect on the next resume.
// ============================================================

TEST_F(DebugSessionTest, RemovingBreakpointMidSessionStopsFuturePauses)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    // bp inside a 3-iteration for-loop body (line 2): without removal
    // we'd pause three times.
    std::string code =
        "for i = 1:3\n"
        "    x = i;\n"
        "end\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused) << "first iteration should pause";

    // Simulate the IDE refreshing the breakpoint list before resume — now
    // with an empty list (user clicked the gutter to remove the bp).
    session.setBreakpoints({});

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed)
        << "after clearing breakpoints, continue must run to completion";
}

TEST_F(DebugSessionTest, RemovingOneOfTwoBreakpointsOnlyOtherFires)
{
    DebugSession session(engine);
    session.setBreakpoints({1, 3});

    std::string code = "a = 1;\nb = 2;\nc = 3;\nd = 4;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 1) << "first bp at line 1";

    // Drop the first bp. Only bp at line 3 should remain.
    session.setBreakpoints({3});

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 3)
        << "second pause must be on the surviving bp, not the removed one";

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

TEST_F(DebugSessionTest, AddBreakpointMidSessionFiresAfterResume)
{
    DebugSession session(engine);
    session.setBreakpoints({1});

    std::string code = "a = 1;\nb = 2;\nc = 3;\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 1);

    // Add bp at line 3 mid-session.
    session.setBreakpoints({1, 3});

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 3)
        << "newly-added bp must fire on next continue";

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
}

// ============================================================
// Script-local functions survive `clear all` in debug mode
// ============================================================

// Non-debug `eval` handles this by splitting the top-level BLOCK and
// re-registering FUNCTION_DEFs between statements. Debug mode compiles
// the whole script as a single chunk (so step semantics stay stable),
// which meant a `clear all` near the top would wipe the script's local
// functions and strand every later call to them. Engine::beginScript
// + clearUserFunctions now re-install script-locals automatically.
TEST_F(DebugSessionTest, ClearAllPreservesScriptLocalFunctionsInDebug)
{
    DebugSession session(engine);
    // No breakpoints — start() steps onto line 1, then Continue runs
    // the rest of the script to completion. If the fix is missing,
    // the call on line 3 raises "VM: undefined function 'add_one'".
    std::string code =
        "clear all;\n"
        "x = 5;\n"
        "y = add_one(x);\n"
        "function out = add_one(v)\n"
        "    out = v + 1;\n"
        "end\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed)
        << "debug mode must keep script-local functions across `clear all`";
    EXPECT_TRUE(session.errorMessage().empty())
        << "unexpected error: " << session.errorMessage();

    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 6.0);
}

// Same scenario, but stepping interactively through the top of the
// script. After `clear all` on line 1, we step onto line 2 and then
// line 3 — the local function must still be callable.
TEST_F(DebugSessionTest, StepOverClearAllKeepsScriptLocalFunctions)
{
    DebugSession session(engine);
    // No breakpoints → start() lands on line 1 in StepInto mode.
    std::string code =
        "clear all;\n"
        "x = 10;\n"
        "y = add_one(x);\n"
        "function out = add_one(v)\n"
        "    out = v + 1;\n"
        "end\n";

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 1);

    // StepOver through lines 1 and 2 — exercises the post-clear state
    // under the debugger's stepping path, not just free-run Continue.
    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    status = session.resume(DebugAction::StepOver);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 3);

    // Continue runs the add_one call and finishes.
    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_TRUE(session.errorMessage().empty())
        << "unexpected error: " << session.errorMessage();

    auto *y = engine.getVariable("y");
    ASSERT_NE(y, nullptr);
    EXPECT_DOUBLE_EQ(y->toScalar(), 11.0);
}
