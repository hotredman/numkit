// tests/diagnostics/debug_session_test.cpp
// Tests for DebugSession: pause/resume, eval in context, stepping
#include "MLabDebugSession.hpp"
#include "MLabEngine.hpp"
#include <gtest/gtest.h>

using namespace mlab;

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

    // First pause is always at line 1 (initial step), then continue to breakpoint
    auto status = startDebug(session, "x = 10;\ny = 20;\nz = 30;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);
}

TEST_F(DebugSessionTest, ContinueToCompletion)
{
    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = startDebug(session, "x = 10;\ny = 20;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Continue past initial step → hits bp at line 2
    status = session.resume(DebugAction::Continue);
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

    auto status = startDebug(session, "for i = 1:3\n  x = i;\nend\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Continue past initial step → hits bp 3 times (once per iteration)
    for (int i = 0; i < 3; ++i) {
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

    // Continue to get inside function (first pause is at top-level)
    status = session.resume(DebugAction::Continue);
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
    status = session.resume(DebugAction::Continue);
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
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Eval an expression using frame variables
    std::string result = session.eval("n + 10");
    EXPECT_NE(result.find("11"), std::string::npos) << "eval('n+10') should show 11, got: " << result;
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

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Continue past initial step → first bp hit: i=1 (before x is assigned)
    status = session.resume(DebugAction::Continue);
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

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Continue past initial step → first bp in double_it(5)
    status = session.resume(DebugAction::Continue);
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

    auto status = startDebug(session, code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Continue past initial step → hits bp at line 2 (plot call)
    status = session.resume(DebugAction::Continue);
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

    // The eval result (or the session output) should contain the figure marker
    std::string out = session.takeOutput();
    bool hasFigure = result.find("__FIGURE_DATA__") != std::string::npos ||
                     out.find("__FIGURE_DATA__") != std::string::npos;
    EXPECT_TRUE(hasFigure)
        << "plot in eval should produce figure marker.\n  result: " << result
        << "\n  output: " << out;

    // Session should still be active
    EXPECT_TRUE(session.isActive());
}
