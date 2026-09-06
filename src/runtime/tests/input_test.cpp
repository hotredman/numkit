// runtime/tests/input_test.cpp
//
// Full-contract guards for `input()` (MATLAB R2025b doc semantics; the
// interactive-only behaviors Windows -batch cannot probe are documented
// in the builtin): both forms, empty-line semantics, expression
// evaluation in the caller's workspace, prompt/type/format/nargout
// validation — dual-engine (VM + TreeWalker) with an injected input
// provider (no pipes in tests).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

class InputTest : public ::testing::TestWithParam<numkit::Engine::Backend> {
public:
    numkit::StandardEngine engine;
    std::vector<std::string> lines;
    std::string displayed;

    void SetUp() override
    {
        engine.setBackend(GetParam());
        engine.setOutputFunc([this](const std::string &s) { displayed += s; });
        engine.setInputProvider([this]() {
            if (lines.empty())
                return std::string();
            std::string front = std::move(lines.front());
            lines.erase(lines.begin());
            return front;
        });
    }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Numeric expression, matrix literal, char literal, function call — the
// line is EVALUATED (form 1).
TEST_P(InputTest, EvaluatesEnteredExpression)
{
    lines = {"4", "[1 2 3]", "'text'", "sqrt(2)", "1e3"};
    eval("x1 = input('p1: ');");
    EXPECT_DOUBLE_EQ(evalScalar("x1"), 4.0);
    eval("x2 = input('p2: ');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(x2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("x2(2)"), 2.0);
    eval("x3 = input('p3: ');");
    EXPECT_EQ(eval("x3;").toString(), "text");
    eval("x4 = input('p4: ');");
    EXPECT_NEAR(evalScalar("x4"), 1.4142135623730951, 1e-15);
    eval("x5 = input('p5: ');");
    EXPECT_DOUBLE_EQ(evalScalar("x5"), 1000.0);
}

// Caller-workspace visibility of the entered expression is the mirror
// half of bugs/opened/core/eval-family-frame-visibility.md (variables
// live in VM registers mid-chunk and reach the Environment only at the
// chunk boundary; the eval builtin has the identical limitation, so
// input is at parity with its sibling). Guarded as
// DISABLED_EvalFamilyCallerVarVisibility in bundle known_bugs_test.

// Empty line: [] in the evaluating form, '' in the 's' form (doc).
TEST_P(InputTest, EmptyLineSemantics)
{
    lines = {"", ""};
    eval("a = input('p: ');");
    EXPECT_DOUBLE_EQ(evalScalar("isempty(a)"), 1.0);
    eval("b = input('p: ', 's');");
    EXPECT_DOUBLE_EQ(evalScalar("isempty(b)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(b)"), 1.0);
}

// 's' form returns the RAW text (no evaluation), preserving spaces.
TEST_P(InputTest, SFormReturnsRawText)
{
    lines = {"[1 2 3]", "hello world"};
    eval("s1 = input('p: ', 's');");
    EXPECT_EQ(eval("s1;").toString(), "[1 2 3]");
    eval("s2 = input('p: ', 's');");
    EXPECT_EQ(eval("s2;").toString(), "hello world");
}

// The prompt goes through the engine output channel; string-scalar
// prompts are accepted like char.
TEST_P(InputTest, PromptDisplayAndStringPrompt)
{
    lines = {"7"};
    displayed.clear();
    eval("v = input('enter N: ', 's');");
    EXPECT_EQ(displayed, "enter N: ");
    lines = {"9"};
    eval("w = input(\"double quoted: \");");
    EXPECT_DOUBLE_EQ(evalScalar("w"), 9.0);
}

// Validation: bad prompt type, bad format, nargout > 1.
TEST_P(InputTest, ArgumentValidation)
{
    lines = {};  // nothing consumed — validation fires first
    EXPECT_THROW(eval("z = input(42);"), std::exception);
    EXPECT_THROW(eval("z = input('p: ', 'q');"), std::exception);
    EXPECT_THROW(eval("z = input('p: ', 'S');"), std::exception);
    EXPECT_THROW(eval("[a, b] = input('p: ');"), std::exception);
}

// An erroneous expression propagates the error (documented divergence:
// interactive MATLAB re-prompts; see the builtin comment).
TEST_P(InputTest, ErroneousExpressionPropagates)
{
    lines = {"undefined_var_xyz"};
    EXPECT_THROW(eval("u = input('p: ');"), std::exception);
    // The engine stays usable afterwards.
    lines = {"5"};
    eval("u = input('p: ');");
    EXPECT_DOUBLE_EQ(evalScalar("u"), 5.0);
}

INSTANTIATE_TEST_SUITE_P(Backends, InputTest,
                         ::testing::Values(numkit::Engine::Backend::VM,
                                           numkit::Engine::Backend::TreeWalker));

} // namespace
