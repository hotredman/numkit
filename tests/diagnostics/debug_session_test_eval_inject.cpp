// Test: variables created in debug eval are accessible after continue
#include <numkit/m/debug/MDebugSession.hpp>
#include <numkit/m/core/MEngine.hpp>
#include <gtest/gtest.h>

using namespace numkit::m;

// Test 1: Modify existing variable during debug, verify change persists
TEST(DebugEvalInjectTest, ModifiedVarSurvivesContinue)
{
    Engine engine;
    std::string output;
    engine.setOutputFunc([&output](const std::string &s) { output += s; });

    DebugSession session(engine);
    session.setBreakpoints({2});

    // x is set to 10 at line 1, breakpoint at line 2
    // At breakpoint, we change x to 999 via eval
    // Line 3 uses the modified x
    std::string code =
        "x = 10;\n"
        "y = 20;\n"
        "disp(x + y);\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    // Modify x in debug console
    session.eval("x = 999");

    // Continue — disp(x + y) should show 999 + 20 = 1019
    output.clear();
    status = session.resume(DebugAction::Continue);
    std::string sessionOut = session.takeOutput();
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_TRUE(session.errorMessage().empty())
        << "Error: " << session.errorMessage();
    std::string allOutput = output + sessionOut;
    EXPECT_NE(allOutput.find("1019"), std::string::npos)
        << "x+y should be 1019, output: [" << output << "] session: [" << sessionOut << "]";
}

// Test 2: New variable created in eval appears in snapshot
TEST(DebugEvalInjectTest, NewVarInSnapshot)
{
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = session.start("x = 1;\ny = 2;\nz = 3;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // Create new variable
    session.eval("q = randn(3,3)");

    // Should appear in snapshot
    auto snap = session.snapshot();
    bool found = false;
    for (auto &v : snap.variables) {
        if (v.name == "q" && v.value) {
            found = true;
            EXPECT_EQ(v.value->dims().rows(), 3u);
            EXPECT_EQ(v.value->dims().cols(), 3u);
        }
    }
    EXPECT_TRUE(found) << "q should appear in snapshot";
}

// Test 3: Eval-created var persists across multiple evals
TEST(DebugEvalInjectTest, NewVarPersistsAcrossEvals)
{
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = session.start("x = 1;\ny = 2;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    session.eval("q = 10");
    std::string result = session.eval("q + 5");
    EXPECT_NE(result.find("15"), std::string::npos)
        << "q+5 should be 15, got: " << result;
}

// Test 4: New undeclared variable created in eval, used after continue
TEST(DebugEvalInjectTest, UndeclaredVarSurvivesContinue)
{
    Engine engine;
    std::string output;
    engine.setOutputFunc([&output](const std::string &s) { output += s; });

    DebugSession session(engine);
    session.setBreakpoints({2});

    // q is NOT in the source code — will be created in debug console
    // Line 3 uses q: if ASSERT_DEF fallback works, q is found in dynVars
    std::string code =
        "x = 10;\n"
        "y = 20;\n"
        "z = x + y + q;\n"
        "disp(z);\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused)
        << "start should hit bp at line 2. Error: [" << session.errorMessage() << "] output: [" << session.takeOutput() << "]";
    EXPECT_EQ(session.snapshot().line, 2);

    // Create q in debug console
    session.eval("q = 100");

    // Continue — z = 10 + 20 + 100 = 130
    output.clear();
    status = session.resume(DebugAction::Continue);
    std::string sessionOut = session.takeOutput();
    std::string allOutput = output + sessionOut;

    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_TRUE(session.errorMessage().empty())
        << "Error: " << session.errorMessage();
    EXPECT_NE(allOutput.find("130"), std::string::npos)
        << "z should be 130, output: [" << allOutput << "]";
}

// Test 5: clear x in debug eval should make x undefined
TEST(DebugEvalInjectTest, ClearVarDuringDebug)
{
    Engine engine;
    std::string output;
    engine.setOutputFunc([&output](const std::string &s) { output += s; });

    DebugSession session(engine);
    session.setBreakpoints({2});

    // x=10 at line 1, bp at line 2
    // At bp: clear x, then continue
    // Line 3: disp(x) should fail because x was cleared
    std::string code =
        "x = 10;\n"
        "y = 20;\n"
        "disp(x);\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    // Verify x is in snapshot with value 10
    auto snap = session.snapshot();
    bool hasX = false;
    for (auto &v : snap.variables)
        if (v.name == "x" && v.value && v.value->toScalar() == 10.0)
            hasX = true;
    EXPECT_TRUE(hasX) << "x should be 10 before clear";

    // Clear x in debug console
    session.eval("clear x");

    // After clear, x should not be in snapshot (or be empty)
    snap = session.snapshot();
    bool xDefined = false;
    for (auto &v : snap.variables)
        if (v.name == "x" && v.value && !v.value->isEmpty())
            xDefined = true;
    EXPECT_FALSE(xDefined) << "x should be cleared";
}

// Test 6: Clear in debug eval does not break continue
TEST(DebugEvalInjectTest, ClearDuringDebugThenContinue)
{
    Engine engine;
    std::string output;
    engine.setOutputFunc([&output](const std::string &s) { output += s; });

    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "x = 10;\n"
        "y = 20;\n"
        "disp(y);\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    // Clear x during debug
    session.eval("clear x");

    // Continue should work — y is still defined, disp(y) should output 20
    output.clear();
    status = session.resume(DebugAction::Continue);
    std::string sessionOut = session.takeOutput();
    std::string allOutput = output + sessionOut;

    EXPECT_EQ(status, ExecStatus::Completed)
        << "Continue after clear should complete. Error: " << session.errorMessage();
    EXPECT_TRUE(session.errorMessage().empty())
        << "No error expected, got: " << session.errorMessage();
    EXPECT_NE(allOutput.find("20"), std::string::npos)
        << "disp(y) should show 20, output: [" << allOutput << "]";
}

// Test 7: Clear all during debug then continue
TEST(DebugEvalInjectTest, ClearAllDuringDebugThenContinue)
{
    Engine engine;
    std::string output;
    engine.setOutputFunc([&output](const std::string &s) { output += s; });

    DebugSession session(engine);
    session.setBreakpoints({3});

    std::string code =
        "a = 1;\n"
        "b = 2;\n"
        "c = 3;\n"
        "disp(c);\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Clear all during debug
    session.eval("clear");

    // Continue — c = 3 is on the paused line, not yet executed
    // After clear, c assignment should still work
    output.clear();
    status = session.resume(DebugAction::Continue);
    std::string sessionOut = session.takeOutput();

    // Should complete (not crash)
    EXPECT_EQ(status, ExecStatus::Completed)
        << "Continue after clear all should complete. Error: " << session.errorMessage();
}

// Test 8: Modified frame var propagates to VM execution
TEST(DebugEvalInjectTest, ModifiedFrameVarInFunction)
{
    Engine engine;
    std::string output;
    engine.setOutputFunc([&output](const std::string &s) { output += s; });

    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "function r = double_it(x)\n"
        "    r = x * 2;\n"
        "end\n"
        "result = double_it(5);\n"
        "disp(result);\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Change x from 5 to 50 inside function
    session.eval("x = 50");

    // Continue — r = x * 2 = 100, disp shows 100
    output.clear();
    status = session.resume(DebugAction::Continue);
    std::string sessionOut = session.takeOutput();
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_TRUE(session.errorMessage().empty())
        << "Error: " << session.errorMessage();
    std::string allOutput = output + sessionOut;
    EXPECT_NE(allOutput.find("100"), std::string::npos)
        << "result should be 100, output: [" << output << "] session: [" << sessionOut << "]";
}

// ============================================================
// MATLAB-style semantics: clearing a variable that the remaining script
// reads must surface "Undefined function or variable" at that line on
// continue (not silently complete, not hang).
// ============================================================

TEST(DebugEvalInjectTest, ClearReferencedVarThenContinueErrors)
{
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({2});

    // bp at line 2; line 3 reads x which the user is about to clear
    std::string code =
        "x = 10;\n"
        "y = 20;\n"
        "z = x + y;\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_EQ(session.snapshot().line, 2);

    session.eval("clear x");

    // After `clear x`, x must not be visible in the snapshot.
    auto snap = session.snapshot();
    for (auto &v : snap.variables) {
        EXPECT_NE(v.name, "x") << "x must be gone after `clear x`";
    }

    status = session.resume(DebugAction::Continue);
    EXPECT_EQ(status, ExecStatus::Completed);
    // Must surface a runtime error — not silent success.
    EXPECT_FALSE(session.errorMessage().empty())
        << "expected undefined-variable error after clear + continue";
    EXPECT_NE(session.errorMessage().find("x"), std::string::npos)
        << "error should mention cleared name 'x', got: " << session.errorMessage();
}

// ============================================================
// Frame variables are written through pointers, no staleness after
// multiple evals.
// ============================================================

TEST(DebugEvalInjectTest, FrameVarWritesSurviveMultipleEvals)
{
    Engine engine;
    std::string output;
    engine.setOutputFunc([&output](const std::string &s) { output += s; });

    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "x = 1;\n"
        "y = 2;\n"
        "disp(x + y);\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);

    // Modify, inspect, modify again — each write lands in the same frame
    // register, so the final value is the one used by the script.
    session.eval("x = 100");
    EXPECT_NE(session.eval("x").find("100"), std::string::npos);
    session.eval("x = 500");
    EXPECT_NE(session.eval("x").find("500"), std::string::npos);

    output.clear();
    status = session.resume(DebugAction::Continue);
    std::string sessionOut = session.takeOutput();
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_TRUE(session.errorMessage().empty());

    std::string all = output + sessionOut;
    EXPECT_NE(all.find("502"), std::string::npos)
        << "expected 500 + 2 = 502 on continue, got: [" << all << "]";
}

// ============================================================
// Overlay: console-created variable is visible to the running script
// after `continue`.
// ============================================================

TEST(DebugEvalInjectTest, OverlayVarVisibleToContinuedScript)
{
    Engine engine;
    std::string output;
    engine.setOutputFunc([&output](const std::string &s) { output += s; });

    DebugSession session(engine);
    session.setBreakpoints({2});

    // q is NOT in the source — user creates it in the debug console.
    std::string code =
        "x = 1;\n"
        "y = 2;\n"
        "disp(x + y + q);\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);

    session.eval("q = 100");

    // Overlay var shows up in the snapshot.
    auto snap = session.snapshot();
    bool sawQ = false;
    for (auto &v : snap.variables) {
        if (v.name == "q" && v.value && v.value->toScalar() == 100.0)
            sawQ = true;
    }
    EXPECT_TRUE(sawQ) << "overlay-created q must appear in snapshot";

    output.clear();
    status = session.resume(DebugAction::Continue);
    std::string sessionOut = session.takeOutput();
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_TRUE(session.errorMessage().empty())
        << "overlay var must be visible to script: " << session.errorMessage();

    std::string all = output + sessionOut;
    EXPECT_NE(all.find("103"), std::string::npos)
        << "expected 1 + 2 + 100 = 103, got: [" << all << "]";
}

// ============================================================
// `clear x` removes a frame variable; a subsequent console `x = ...`
// re-creates it (either in the frame — write-through — or in overlay).
// Either way, continue reads the new value.
// ============================================================

// ============================================================
// Built-in constants (pi, eps, Inf, NaN, i, j, true, false, end, ans,
// nargin, nargout) must not appear in the debug snapshot even if the
// script references them.
// ============================================================

// ============================================================
// MATLAB-style built-in shadowing at debug pauses.
// ============================================================

// Helper: does snapshot contain a variable with the given name (not deleted)?
static bool snapshotHas(const DebugSession::Snapshot &snap, const std::string &name)
{
    for (auto &v : snap.variables)
        if (v.name == name && v.value && !v.value->isDeleted())
            return true;
    return false;
}

TEST(DebugEvalInjectTest, ScriptShadowsBuiltinVisibleInDebug)
{
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({2});

    // Script assigns to `pi` — from that point pi is a user variable.
    std::string code =
        "pi = 5;\n"
        "x = pi + 1;\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    EXPECT_TRUE(snapshotHas(snap, "pi"))
        << "after `pi = 5` the debug snapshot must list pi";
    // And the value is the user-supplied one.
    for (auto &v : snap.variables)
        if (v.name == "pi")
            EXPECT_DOUBLE_EQ(v.value->toScalar(), 5.0);
}

TEST(DebugEvalInjectTest, ScriptReadsBuiltinHiddenInDebug)
{
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({2});

    // Script only reads pi — must not appear in debug snapshot.
    std::string code =
        "x = pi;\n"
        "y = x + 1;\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    EXPECT_FALSE(snapshotHas(snap, "pi"))
        << "pi is only read, not assigned — must be hidden in snapshot";
}

TEST(DebugEvalInjectTest, ConsoleShadowOfBuiltinVisibleAfterwards)
{
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({1});

    // Script never touches pi.
    auto status = session.start("x = 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_FALSE(snapshotHas(session.snapshot(), "pi"))
        << "pi not mentioned by script — must be hidden";

    // User shadows pi via the console.
    session.eval("pi = 7");
    EXPECT_TRUE(snapshotHas(session.snapshot(), "pi"))
        << "console `pi = 7` must make pi visible in snapshot";

    // The stored value is the user-supplied one.
    for (auto &v : session.snapshot().variables)
        if (v.name == "pi")
            EXPECT_DOUBLE_EQ(v.value->toScalar(), 7.0);
}

TEST(DebugEvalInjectTest, ConsoleShadowPersistsAcrossResume)
{
    Engine engine;
    std::string output;
    engine.setOutputFunc([&output](const std::string &s) { output += s; });

    DebugSession session(engine);
    session.setBreakpoints({1, 2});

    // Two bp stops; pi is not in the script, so it is hidden on the first
    // pause and must stay visible on the second pause after a console
    // shadow.
    auto status = session.start("x = 1;\ny = 2;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    session.eval("pi = 11");
    EXPECT_TRUE(snapshotHas(session.snapshot(), "pi"));

    // Continue → hit bp on line 2.
    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused);
    EXPECT_TRUE(snapshotHas(session.snapshot(), "pi"))
        << "console-shadowed pi must persist across a resume";
}

// User-reported: in debug mode, typing a bare expression like `cos(10)` in
// the console must put `ans` into the debug snapshot (= the Workspace panel
// of the IDE), with the computed value. Built-in constants referenced by
// the paused script must NOT leak into the snapshot.
TEST(DebugEvalInjectTest, ConsoleBareExpressionMakesAnsVisible)
{
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = session.start("x = 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    // ans must not be there yet.
    auto before = session.snapshot();
    for (auto &v : before.variables)
        EXPECT_NE(v.name, "ans") << "pre-console snapshot should not list ans";

    session.eval("cos(10)");

    auto after = session.snapshot();
    bool sawAns = false;
    double ansVal = 0;
    for (auto &v : after.variables) {
        if (v.name == "ans" && v.value) {
            sawAns = true;
            ansVal = v.value->toScalar();
        }
    }
    EXPECT_TRUE(sawAns) << "after `cos(10)` in console, ans must be in snapshot";
    EXPECT_NEAR(ansVal, std::cos(10.0), 1e-12);
}

TEST(DebugEvalInjectTest, ConsoleBareExpressionDoesNotExposeReferencedBuiltins)
{
    // The script touches `pi`, which therefore ends up in varMap via
    // preImport. A console bare-expression must not incidentally surface
    // `pi` (or any other reserved name) in the snapshot — only the
    // `ans` it actually set.
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({2});

    auto status = session.start("r = pi;\ns = r + 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    session.eval("cos(10)");

    auto snap = session.snapshot();
    for (auto &v : snap.variables) {
        EXPECT_NE(v.name, "pi")
            << "pi was only read by the script, must not show up in snapshot";
    }
    bool sawAns = false;
    for (auto &v : snap.variables)
        if (v.name == "ans") sawAns = true;
    EXPECT_TRUE(sawAns) << "ans from console eval must be in snapshot";
}

TEST(DebugEvalInjectTest, ConsoleShadowRoundTrip)
{
    // pi shadowed via console then cleared — must no longer show up in the
    // snapshot. Guards against shadowedBuiltins_ accumulating stale entries.
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({1});

    auto status = session.start("x = 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);
    ASSERT_FALSE(snapshotHas(session.snapshot(), "pi"));

    session.eval("pi = 11");
    ASSERT_TRUE(snapshotHas(session.snapshot(), "pi")) << "shadow must appear";

    session.eval("clear pi");
    EXPECT_FALSE(snapshotHas(session.snapshot(), "pi"))
        << "after clear pi in console, pi must disappear from snapshot";
}

TEST(DebugEvalInjectTest, NarginNotShownInSnapshotButReachable)
{
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({2});

    // nargin is a pseudo-var, always allocated by the compiler in function
    // chunks. It must be reachable from console eval but not visible in
    // the snapshot.
    std::string code =
        "function r = add(a, b)\n"
        "    r = a + b;\n"
        "end\n"
        "r = add(10, 20);\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);

    EXPECT_FALSE(snapshotHas(session.snapshot(), "nargin"))
        << "nargin must be hidden from snapshot";

    // But console eval can read it:
    std::string result = session.eval("nargin");
    EXPECT_NE(result.find("2"), std::string::npos)
        << "eval('nargin') must still resolve to 2, got: " << result;
}

TEST(DebugEvalInjectTest, BuiltinConstantsHiddenFromWorkspace)
{
    Engine engine;
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({3});

    // Script touches pi and eps; compiler will allocate registers for them
    // and put them in varMap. Debug snapshot must still filter them out.
    std::string code =
        "x = pi;\n"
        "y = eps;\n"
        "z = x + y;\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    for (auto &v : snap.variables) {
        EXPECT_NE(v.name, "pi")  << "pi is a built-in, should not show up";
        EXPECT_NE(v.name, "eps") << "eps is a built-in, should not show up";
        EXPECT_NE(v.name, "ans") << "ans is a pseudo-var, should not show up";
    }
}

TEST(DebugEvalInjectTest, ClearThenReassignSurvivesContinue)
{
    Engine engine;
    std::string output;
    engine.setOutputFunc([&output](const std::string &s) { output += s; });

    DebugSession session(engine);
    session.setBreakpoints({2});

    std::string code =
        "x = 10;\n"
        "y = 20;\n"
        "disp(x + y);\n";

    auto status = session.start(code);
    ASSERT_EQ(status, ExecStatus::Paused);

    session.eval("clear x");
    session.eval("x = 7");

    output.clear();
    status = session.resume(DebugAction::Continue);
    std::string sessionOut = session.takeOutput();
    EXPECT_EQ(status, ExecStatus::Completed);
    EXPECT_TRUE(session.errorMessage().empty())
        << "after clear + reassign, continue must succeed: " << session.errorMessage();

    std::string all = output + sessionOut;
    EXPECT_NE(all.find("27"), std::string::npos)
        << "expected 7 + 20 = 27 on continue, got: [" << all << "]";
}
