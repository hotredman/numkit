// tests/gtest/integration/multi_output_args_test.cpp
//
// Multi-output destructure (`[a,b,c] = f(...)`) semantics, on both
// backends. Locks in bug #44:
//   (a) a destructured local must be a variable on a later index
//       (`[r,c,v] = find(G); c(k)`), not a phantom function lookup;
//   (b) requesting more outputs than the callee produces is reported as
//       "Too many output arguments" at the call, not a downstream
//       "undefined function" when the unassigned target is referenced.

#include "dual_engine_fixture.hpp"
#include <string>

using namespace m_test;
using namespace numkit;

class MultiOutputArgs : public DualEngineTest
{
protected:
    // Returns the exception message (empty if no throw).
    std::string evalErr(const std::string &code)
    {
        try {
            engine.eval(code);
            return {};
        } catch (const std::exception &e) {
            return e.what();
        }
    }
};

// ── (a) destructured locals resolve as variables, not functions ──
TEST_P(MultiOutputArgs, DestructuredLocalIndexes)
{
    eval("clear; G = [1 0; 0 2]; [r, c, v] = find(G);");
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 1.0);
}

TEST_P(MultiOutputArgs, DestructuredLocalInForLoop)
{
    // The graycomatrix pattern: short-named destructure output indexed
    // inside a top-level for-loop.
    eval("clear; G = [5 0; 0 7]; [r, c, v] = find(G); s = 0;"
         " for k = 1:numel(v); s = s + v(k); end;");
    EXPECT_DOUBLE_EQ(evalScalar("s"), 12.0);
}

TEST_P(MultiOutputArgs, DestructureWithTildeThenIndex)
{
    eval("clear; [~, c] = find([0 3]); y = c(1);");
    EXPECT_DOUBLE_EQ(evalScalar("y"), 2.0);
}

// ── (b) too many outputs → clear error at the call ──
TEST_P(MultiOutputArgs, TooManyFromBuiltin)
{
    std::string err = evalErr("clear; [a, b] = sin(1);");
    EXPECT_NE(err.find("Too many output arguments"), std::string::npos) << err;
}

TEST_P(MultiOutputArgs, TooManyFromUserFunction)
{
    std::string err = evalErr("clear; function a = g(); a = 1; end\n[x, y] = g();");
    EXPECT_NE(err.find("Too many output arguments"), std::string::npos) << err;
}

TEST_P(MultiOutputArgs, TooManySingleBracketVoid)
{
    // requesting an output from a no-output function
    std::string err = evalErr("clear; function h(); end\n[a] = h();");
    EXPECT_NE(err.find("Too many output arguments"), std::string::npos) << err;
}

// ── requesting fewer / exact is fine ──
TEST_P(MultiOutputArgs, ExactOutputsOK)
{
    eval("clear; [a, b] = size(zeros(2, 3));");
    EXPECT_DOUBLE_EQ(evalScalar("a"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("b"), 3.0);
}

TEST_P(MultiOutputArgs, FewerOutputsOK)
{
    // size yields 2 outputs; requesting 1 is allowed — `a` is the [r c]
    // size row vector (MATLAB semantics), not an error.
    eval("clear; [a] = size(zeros(2, 3));");
    EXPECT_DOUBLE_EQ(evalScalar("a(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("a(2)"), 3.0);
}

TEST_P(MultiOutputArgs, TildeStillCountsButProduced)
{
    // max yields 2 (value, index); [~, idx] requests 2 — fine.
    eval("clear; [~, idx] = max([3 9 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("idx"), 2.0);
}

INSTANTIATE_DUAL(MultiOutputArgs);
