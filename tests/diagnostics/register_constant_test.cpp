// tests/diagnostics/register_constant_test.cpp
//
// Locks down the host-side API Engine::registerConstant(name, val).
// The registered name must behave exactly like the built-in `pi`/`eps`
// constants — served from constantsEnv_, hidden from `whos` and the
// debug Workspace panel, shadowable, un-shadowable via `clear name`,
// and surviving `clear all`.

#include "MDebugSession.hpp"
#include "dual_engine_fixture.hpp"

using namespace mlab_test;
using namespace mlab;

class RegisterConstantTest : public DualEngineTest
{
protected:
    void SetUp() override
    {
        DualEngineTest::SetUp();
        engine.registerConstant("G", MValue::scalar(6.674e-11, &engine.allocator()));
    }

    bool workspaceHas(const std::string &name)
    {
        auto names = engine.workspaceVarNames();
        return std::find(names.begin(), names.end(), name) != names.end();
    }
};

// ============================================================
// Read from scripts — constant resolves like a built-in.
// ============================================================

TEST_P(RegisterConstantTest, ReadableFromScript)
{
    EXPECT_NEAR(evalScalar("G;"), 6.674e-11, 1e-20);
}

TEST_P(RegisterConstantTest, UsableInExpression)
{
    EXPECT_NEAR(evalScalar("2 * G;"), 2 * 6.674e-11, 1e-20);
}

TEST_P(RegisterConstantTest, UsableInsideFunction)
{
    eval(R"(
        function r = scaled(x)
            r = x * G;
        end
    )");
    EXPECT_NEAR(evalScalar("scaled(3);"), 3 * 6.674e-11, 1e-20);
}

// ============================================================
// Not visible to the user-workspace surfaces.
// ============================================================

TEST_P(RegisterConstantTest, NotListedInWhos)
{
    eval("x = 1;");
    capturedOutput.clear();
    eval("whos");
    EXPECT_NE(capturedOutput.find("x"), std::string::npos);
    EXPECT_EQ(capturedOutput.find("G"), std::string::npos)
        << "registered constant G must not appear in whos; got: " << capturedOutput;
}

TEST_P(RegisterConstantTest, NotInWorkspaceVarNames)
{
    EXPECT_FALSE(workspaceHas("G"))
        << "registered constant must not be in workspaceVarNames before any shadow";
}

TEST_P(RegisterConstantTest, ReadingDoesNotLeakIntoWorkspace)
{
    // Reading G must not add it to the local base workspace, exactly like
    // reading pi doesn't add pi.
    eval("y = G + 1;");
    EXPECT_TRUE(workspaceHas("y"));
    EXPECT_FALSE(workspaceHas("G"))
        << "reading G in an expression must NOT create a workspace local";
}

// ============================================================
// Shadow + clear round-trip — same lifecycle as built-ins.
// ============================================================

TEST_P(RegisterConstantTest, ShadowViaAssignmentIsVisible)
{
    eval("G = 123;");
    EXPECT_TRUE(workspaceHas("G"))
        << "after `G = 123;`, shadow must appear in workspace (MATLAB behaviour)";
    EXPECT_DOUBLE_EQ(evalScalar("G;"), 123.0);
}

TEST_P(RegisterConstantTest, ClearNameUnShadows)
{
    eval("G = 123;");
    ASSERT_TRUE(workspaceHas("G"));
    eval("clear G");
    EXPECT_FALSE(workspaceHas("G"));
    EXPECT_NEAR(evalScalar("G;"), 6.674e-11, 1e-20)
        << "after `clear G`, subsequent reads must resolve back to the constant";
}

// ============================================================
// Survival under `clear all`.
// ============================================================

TEST_P(RegisterConstantTest, SurvivesClearAll)
{
    eval("clear all");
    EXPECT_NEAR(evalScalar("G;"), 6.674e-11, 1e-20)
        << "host-registered constant must survive `clear all`";
}

TEST_P(RegisterConstantTest, ShadowThenClearAllRestoresConstant)
{
    eval("G = 42;");
    EXPECT_DOUBLE_EQ(evalScalar("G;"), 42.0);
    eval("clear all");
    EXPECT_NEAR(evalScalar("G;"), 6.674e-11, 1e-20)
        << "`clear all` must strip the shadow and restore the registered value";
}

// ============================================================
// Compiler behaviour: registered constant should be treated as a
// reserved name. Reading it must not fall into the ASSERT_DEF /
// "Undefined variable" path used for generic identifiers, exactly
// like pi.
// ============================================================

TEST_P(RegisterConstantTest, NoUndefinedVariableErrorOnBareRead)
{
    // Bare `G` — if the compiler treated G as an unknown identifier,
    // ASSERT_DEF would fire at runtime and produce
    // "Undefined function or variable 'G'".
    capturedOutput.clear();
    eval("G");
    EXPECT_EQ(capturedOutput.find("Undefined"), std::string::npos)
        << "bare read of a registered constant must not error; got: "
        << capturedOutput;
    EXPECT_NE(capturedOutput.find("ans"), std::string::npos);
}

INSTANTIATE_DUAL(RegisterConstantTest);

// ============================================================
// Debug snapshot — the Workspace panel during a debug pause must
// also hide registered constants (matching `whos`). Not parametrised
// because DebugSession is VM-only.
// ============================================================

TEST(RegisterConstantDebugTest, HiddenFromDebugSnapshot)
{
    Engine engine;
    StdLibrary::install(engine);
    engine.registerConstant("G", MValue::scalar(6.674e-11, &engine.allocator()));
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({2});

    // Script reads G — ensures G ends up in varMap (pre-import path),
    // which is exactly where hidden-by-default filtering needs to kick in.
    auto status = session.start("x = G;\ny = x + 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    for (auto &v : snap.variables) {
        EXPECT_NE(v.name, "G")
            << "G was only read by the script — must not show up in debug snapshot";
    }
}

TEST(RegisterConstantDebugTest, ShadowingRegisteredConstantInScriptShowsInSnapshot)
{
    Engine engine;
    StdLibrary::install(engine);
    engine.registerConstant("G", MValue::scalar(6.674e-11, &engine.allocator()));
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({2});

    // Script assigns G → user shadow → snapshot should list it with the
    // new value.
    auto status = session.start("G = 99;\nx = 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    bool sawG = false;
    for (auto &v : snap.variables) {
        if (v.name == "G" && v.value) {
            sawG = true;
            EXPECT_DOUBLE_EQ(v.value->toScalar(), 99.0);
        }
    }
    EXPECT_TRUE(sawG)
        << "after `G = 99;` in the script, debug snapshot must list G (shadow)";
}

TEST(RegisterConstantDebugTest, ShadowingFromConsoleShowsInSnapshot)
{
    Engine engine;
    StdLibrary::install(engine);
    engine.registerConstant("G", MValue::scalar(6.674e-11, &engine.allocator()));
    engine.setOutputFunc([](const std::string &) {});

    DebugSession session(engine);
    session.setBreakpoints({1});

    // Script doesn't touch G. User shadows via the debug console.
    auto status = session.start("x = 1;\n");
    ASSERT_EQ(status, ExecStatus::Paused);

    auto snap = session.snapshot();
    for (auto &v : snap.variables)
        EXPECT_NE(v.name, "G") << "G not yet shadowed — must be hidden";

    session.eval("G = 7");

    snap = session.snapshot();
    bool sawG = false;
    for (auto &v : snap.variables) {
        if (v.name == "G" && v.value) {
            sawG = true;
            EXPECT_DOUBLE_EQ(v.value->toScalar(), 7.0);
        }
    }
    EXPECT_TRUE(sawG)
        << "console shadow `G = 7` must make G visible in snapshot";
}
