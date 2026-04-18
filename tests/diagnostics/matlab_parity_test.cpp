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

INSTANTIATE_DUAL(MatlabParity);
