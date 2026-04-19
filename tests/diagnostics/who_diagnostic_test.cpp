// tests/test_who_diagnostic.cpp
#include "dual_engine_fixture.hpp"

using namespace mlab_test;
using namespace numkit::m::m;

class WhoDiagnostic : public DualEngineTest
{};

TEST_P(WhoDiagnostic, WhoSeesVarsFromPreviousEval)
{
    eval("diag_x = 42;");
    eval("diag_y = 7;");
    capturedOutput.clear();
    eval("who");

    std::cerr << "DIAGNOSTIC WHO-1 [" << (GetParam() == BackendParam::VM ? "VM" : "TW")
              << "]: output = '" << capturedOutput << "'\n";

    EXPECT_TRUE(capturedOutput.find("diag_x") != std::string::npos)
        << "who should show diag_x from previous eval";
    EXPECT_TRUE(capturedOutput.find("diag_y") != std::string::npos)
        << "who should show diag_y from previous eval";
}

TEST_P(WhoDiagnostic, WhoSeesVarsAfterError)
{
    try {
        eval("err_a = 100;\nerr_b = 200;\nv = [1 2 3];\nv(999);\n");
    } catch (...) {}

    capturedOutput.clear();
    eval("who");

    std::cerr << "DIAGNOSTIC WHO-2 [" << (GetParam() == BackendParam::VM ? "VM" : "TW")
              << "]: output = '" << capturedOutput << "'\n";

    EXPECT_TRUE(capturedOutput.find("err_a") != std::string::npos)
        << "who should show err_a after error";
    EXPECT_TRUE(capturedOutput.find("err_b") != std::string::npos)
        << "who should show err_b after error";
}

// ============================================================
// MATLAB-style built-in shadowing: `pi = 5` at the REPL shadows the
// built-in constant and from that point onwards appears in who/whos
// and in workspaceVarNames().
// ============================================================

// A bare read of a built-in — for example typing `pi` at the REPL — must
// NOT create a workspace local. Previously the VM exported every varMap
// entry into lastVarMap_ regardless of whether the chunk wrote to it, so
// `pi` would appear in `who` and workspaceVarNames after a read-only use.
TEST_P(WhoDiagnostic, BareReadDoesNotCreateWorkspaceLocal)
{
    eval("pi");        // no semicolon: triggers display, still just a read
    capturedOutput.clear();
    eval("who");
    EXPECT_EQ(capturedOutput.find("pi"), std::string::npos)
        << "reading pi must not make it a workspace variable; got: "
        << capturedOutput;

    auto names = engine.workspaceVarNames();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "pi") == names.end())
        << "workspaceVarNames must not list pi after a read";
}

TEST_P(WhoDiagnostic, ReadingUserVarDoesNotReExportIt)
{
    // Establish a user variable, then run a chunk that only reads it.
    eval("x = 42;");
    eval("y = x + 1;");   // reads x, writes y
    auto names = engine.workspaceVarNames();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "x") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "y") != names.end());
    // x's value must still be the original — not clobbered by re-export.
    capturedOutput.clear();
    eval("disp(x);");
    EXPECT_NE(capturedOutput.find("42"), std::string::npos)
        << "x must remain 42 after being read by another chunk; got: "
        << capturedOutput;
}

TEST_P(WhoDiagnostic, BuiltinHiddenBeforeShadow)
{
    // Clean workspace — pi must not be in `who`, `whos`, or
    // workspaceVarNames().
    capturedOutput.clear();
    eval("who");
    EXPECT_EQ(capturedOutput.find("pi"), std::string::npos)
        << "clean workspace should not list pi in `who`; got: " << capturedOutput;

    capturedOutput.clear();
    eval("whos");
    EXPECT_EQ(capturedOutput.find("pi"), std::string::npos)
        << "clean workspace should not list pi in `whos`; got: " << capturedOutput;

    auto names = engine.workspaceVarNames();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "pi") == names.end())
        << "workspaceVarNames() must not list pi before any assignment";
}

TEST_P(WhoDiagnostic, BuiltinVisibleAfterShadow)
{
    eval("pi = 5;");

    capturedOutput.clear();
    eval("who");
    EXPECT_NE(capturedOutput.find("pi"), std::string::npos)
        << "after `pi = 5`, `who` must list pi; got: " << capturedOutput;

    capturedOutput.clear();
    eval("whos");
    EXPECT_NE(capturedOutput.find("pi"), std::string::npos)
        << "after `pi = 5`, `whos` must list pi; got: " << capturedOutput;

    auto names = engine.workspaceVarNames();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "pi") != names.end())
        << "workspaceVarNames() must list pi after shadowing";

    // The shadowed value must be what the user wrote.
    capturedOutput.clear();
    eval("disp(pi);");
    EXPECT_NE(capturedOutput.find("5"), std::string::npos)
        << "disp(pi) after shadow must show 5; got: " << capturedOutput;
}

TEST_P(WhoDiagnostic, BuiltinHiddenAgainAfterClear)
{
    eval("pi = 5;");

    capturedOutput.clear();
    eval("who");
    ASSERT_NE(capturedOutput.find("pi"), std::string::npos) << "sanity: pi visible after shadow";

    eval("clear pi");

    capturedOutput.clear();
    eval("who");
    EXPECT_EQ(capturedOutput.find("pi"), std::string::npos)
        << "after `clear pi`, pi must be hidden again; got: " << capturedOutput;

    auto names = engine.workspaceVarNames();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "pi") == names.end())
        << "workspaceVarNames() must not list pi after clear";

    // And `disp(pi)` must return the built-in value again.
    capturedOutput.clear();
    eval("disp(pi);");
    EXPECT_NE(capturedOutput.find("3.14"), std::string::npos)
        << "disp(pi) after clear must show the built-in ~3.14; got: "
        << capturedOutput;
}

INSTANTIATE_DUAL(WhoDiagnostic);