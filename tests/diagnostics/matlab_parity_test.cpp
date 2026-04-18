// tests/diagnostics/matlab_parity_test.cpp
//
// Locks in MATLAB-parity behaviour for three related surfaces:
//   1. `ans` semantics — every bare-expression statement writes the
//      result into `ans`, with or without a trailing semicolon;
//      assignments do not.
//   2. Bare-identifier display label — `v` on a line displays using the
//      variable's own name, not "ans"; built-ins and computed expressions
//      still display as "ans".
//   3. `nargin` / `nargout` at the top level — reading either outside a
//      function throws MATLAB's exact message, not a generic
//      "Undefined function or variable".

#include "dual_engine_fixture.hpp"

using namespace mlab_test;
using namespace mlab;

class MatlabParity : public DualEngineTest
{
protected:
    bool hasVar(const std::string &name)
    {
        auto names = engine.workspaceVarNames();
        return std::find(names.begin(), names.end(), name) != names.end();
    }

    double getVar(const std::string &name)
    {
        auto *v = engine.workspaceEnv().get(name);
        return (v && !v->isEmpty()) ? v->toScalar() : 0.0;
    }
};

// ============================================================
// #1 — `ans` semantics
// ============================================================

TEST_P(MatlabParity, AnsSetByBareExpressionNoSemicolon)
{
    capturedOutput.clear();
    eval("5 + 3");
    EXPECT_NE(capturedOutput.find("ans"), std::string::npos)
        << "display should use 'ans'; got: " << capturedOutput;
    EXPECT_TRUE(hasVar("ans")) << "ans must become a workspace variable";
    EXPECT_DOUBLE_EQ(getVar("ans"), 8.0);
}

TEST_P(MatlabParity, AnsSetByBareExpressionWithSemicolon)
{
    capturedOutput.clear();
    eval("5 + 3;");
    EXPECT_EQ(capturedOutput.find("ans"), std::string::npos)
        << "semicolon suppresses display; got: " << capturedOutput;
    EXPECT_TRUE(hasVar("ans")) << "ans is still stored even with ';'";
    EXPECT_DOUBLE_EQ(getVar("ans"), 8.0);
}

TEST_P(MatlabParity, AnsSetByFunctionCallWithSemicolon)
{
    capturedOutput.clear();
    eval("sin(0.5);");
    EXPECT_TRUE(hasVar("ans")) << "sin(...) is an anonymous value — ans updates";
    EXPECT_NEAR(getVar("ans"), std::sin(0.5), 1e-12);
}

TEST_P(MatlabParity, AnsSetByBareBuiltinConstant)
{
    capturedOutput.clear();
    eval("pi");
    EXPECT_NE(capturedOutput.find("ans"), std::string::npos);
    EXPECT_TRUE(hasVar("ans"));
    EXPECT_NEAR(getVar("ans"), M_PI, 1e-10);
}

TEST_P(MatlabParity, AnsNotSetByAssignment)
{
    eval("x = 5;");
    EXPECT_FALSE(hasVar("ans"))
        << "plain assignment x = 5 must not create ans";
    EXPECT_TRUE(hasVar("x"));
}

TEST_P(MatlabParity, AnsPersistsAcrossChunks)
{
    eval("5 + 3;");       // ans = 8
    eval("y = ans + 1;"); // reads ans from prior chunk
    EXPECT_DOUBLE_EQ(getVar("y"), 9.0);
}

TEST_P(MatlabParity, AnsRebindsEachEval)
{
    eval("1 + 1;"); // ans = 2
    EXPECT_DOUBLE_EQ(getVar("ans"), 2.0);
    eval("10 * 10;"); // ans = 100
    EXPECT_DOUBLE_EQ(getVar("ans"), 100.0);
}

// ============================================================
// #2 — Bare identifier display label
// ============================================================

TEST_P(MatlabParity, BareUserVarDisplaysByName)
{
    eval("v = [1 3 4];"); // silent
    capturedOutput.clear();
    eval("v");
    EXPECT_NE(capturedOutput.find("v "), std::string::npos)
        << "display header should be 'v =' not 'ans ='; got: "
        << capturedOutput;
    EXPECT_EQ(capturedOutput.find("ans"), std::string::npos)
        << "bare read of a user variable must not display 'ans'; got: "
        << capturedOutput;
}

TEST_P(MatlabParity, BareUserVarDoesNotOverwriteAns)
{
    eval("5 + 3;"); // ans = 8
    eval("v = 42;");
    eval("v");      // display v; must NOT clobber ans
    EXPECT_DOUBLE_EQ(getVar("ans"), 8.0)
        << "bare read of v must not reassign ans";
}

TEST_P(MatlabParity, ShadowedBuiltinDisplaysByName)
{
    eval("pi = 5;"); // pi becomes a workspace local
    capturedOutput.clear();
    eval("pi");
    EXPECT_NE(capturedOutput.find("pi "), std::string::npos)
        << "after shadow, pi is a user variable — display by name; got: "
        << capturedOutput;
}

TEST_P(MatlabParity, BuiltinBeforeShadowStillUsesAns)
{
    // pi with no prior assignment — still treated as an anonymous value.
    capturedOutput.clear();
    eval("eps");
    EXPECT_NE(capturedOutput.find("ans"), std::string::npos);
    EXPECT_TRUE(hasVar("ans"));
}

// ============================================================
// #3 — `nargin` / `nargout` at the top level
// ============================================================

TEST_P(MatlabParity, NarginAtTopLevelErrors)
{
    try {
        eval("nargin");
        FAIL() << "expected an exception at top-level nargin";
    } catch (const std::exception &e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("nargin/nargout"), std::string::npos)
            << "error should mention nargin/nargout; got: " << msg;
        EXPECT_NE(msg.find("within a MATLAB function"), std::string::npos)
            << "error should be the MATLAB-specific message; got: " << msg;
    }
}

TEST_P(MatlabParity, NargoutAtTopLevelErrors)
{
    try {
        eval("nargout");
        FAIL() << "expected an exception at top-level nargout";
    } catch (const std::exception &e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("nargin/nargout"), std::string::npos);
    }
}

TEST_P(MatlabParity, NarginInsideFunctionWorks)
{
    eval(R"(
        function r = nargin_probe(a, b, c)
            r = nargin;
        end
    )");
    EXPECT_DOUBLE_EQ(evalScalar("nargin_probe(1, 2, 3);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("nargin_probe(1, 2);"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("nargin_probe(1);"), 1.0);
}

// ============================================================
// Multi-statement script parity — every top-level statement commits
// its effects to the workspace before the next one runs, so mid-script
// `whos` / `clear x` behave the same as they do line-by-line in the
// REPL. This mirrors MATLAB's script semantics.
// ============================================================

TEST_P(MatlabParity, MultiStatementClearUnshadowsBuiltin)
{
    // Single eval, three statements. Without per-statement sync the
    // `clear pi` can't touch the still-live chunk register, so the third
    // disp would see 5 instead of the built-in value.
    capturedOutput.clear();
    eval(R"(
        pi = 5;
        disp(pi);
        clear pi;
        disp(pi);
    )");
    // Both displays appear in order: 5 first, then the built-in 3.14…
    auto posFive = capturedOutput.find("5");
    auto posPi   = capturedOutput.find("3.14");
    ASSERT_NE(posFive, std::string::npos) << "got: " << capturedOutput;
    ASSERT_NE(posPi,   std::string::npos) << "got: " << capturedOutput;
    EXPECT_LT(posFive, posPi) << "5 must come before 3.14…; got: " << capturedOutput;
}

TEST_P(MatlabParity, MultiStatementWhosSeesPriorAssignments)
{
    // `whos` in the middle of a script must see variables assigned by
    // preceding top-level statements in the same eval.
    capturedOutput.clear();
    eval(R"(
        a = 1;
        b = 2;
        whos
    )");
    EXPECT_NE(capturedOutput.find("a"), std::string::npos)
        << "mid-script whos must list a; got: " << capturedOutput;
    EXPECT_NE(capturedOutput.find("b"), std::string::npos)
        << "mid-script whos must list b; got: " << capturedOutput;
}

TEST_P(MatlabParity, MultiStatementClearNamedRemovesVar)
{
    eval(R"(
        a = 1; b = 2; c = 3;
        clear b
    )");
    auto names = engine.workspaceVarNames();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "a") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "b") == names.end())
        << "clear b inside multi-statement eval must remove b";
    EXPECT_TRUE(std::find(names.begin(), names.end(), "c") != names.end());
}

TEST_P(MatlabParity, FunctionDefsAreForwardReferenceable)
{
    // Call the function at the top of the script, define it lower down.
    // MATLAB (and now MLab's split execution with a pre-registration
    // pass) allows this because the AST gets scanned for FUNCTION_DEF
    // nodes before any statement runs.
    eval(R"(
        r = triple(4);
        function y = triple(x)
            y = x * 3;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("r"), 12.0);
}

TEST_P(MatlabParity, LoopAsSingleStatementIsOneChunk)
{
    // Control-flow constructs are a single top-level statement — the
    // body runs in one chunk, so `break` / `continue` still work.
    eval(R"(
        s = 0;
        for i = 1:5
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("s"), 15.0);
    EXPECT_DOUBLE_EQ(getVar("i"), 5.0);
}

TEST_P(MatlabParity, WhileSimpleSplit)
{
    eval(R"(
        i = 0;
        while i < 3
            i = i + 1;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("i"), 3.0);
}

TEST_P(MatlabParity, WhileAloneNoSplit)
{
    // One statement at top-level — should NOT split (size() == 1 after
    // the i=0 is stashed via setVariable rather than eval).
    engine.setVariable("i", mlab::MValue::scalar(0));
    eval(R"(
        while i < 3
            i = i + 1;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("i"), 3.0);
}

TEST_P(MatlabParity, WhileWithContinueInsideIf_MinimalRepro)
{
    eval(R"(
        i = 0;
        s = 0;
        while i < 3
            i = i + 1;
            if mod(i, 2) == 0
                continue;
            end
            s = s + i;
        end
    )");
    EXPECT_DOUBLE_EQ(getVar("s"), 4.0);  // 1 + 3
}

INSTANTIATE_DUAL(MatlabParity);
