// Test: variables created in debug eval are accessible after continue
#include "MLabDebugSession.hpp"
#include "MLabEngine.hpp"
#include <gtest/gtest.h>

using namespace mlab;

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

    status = session.resume(DebugAction::Continue);
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
    status = session.resume(DebugAction::Continue);
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
    status = session.resume(DebugAction::Continue);
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
    // Check if compilation/start succeeded
    ASSERT_EQ(status, ExecStatus::Paused)
        << "start should pause. Error: [" << session.errorMessage() << "] output: [" << session.takeOutput() << "]";
    ASSERT_EQ(status, ExecStatus::Paused) << "start should pause, error: " << session.errorMessage();

    status = session.resume(DebugAction::Continue);
    ASSERT_EQ(status, ExecStatus::Paused) << "should hit bp at line 2, error: " << session.errorMessage();
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

// Test 5: Modified frame var propagates to VM execution
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
    status = session.resume(DebugAction::Continue);
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
