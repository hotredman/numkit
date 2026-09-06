// bundle/tests/clear_semantics_test.cpp
//
// `clear` semantics vs MATLAB R2025b (live regression guard for
// bugs/closed/runtime/clear-all-wipes-builtin-msources.md):
//   - `clear all` / `clear classes` / `clear functions` unloads functions
//     and classes in MATLAB, but they RELOAD TRANSPARENTLY from disk on
//     the next call — engine-startup m-source functions (inline,
//     impulse(b,a,t), ode45, …) and the inline classdef must stay
//     callable (reinstallBuiltinSources re-registers them; they have no
//     disk backing).
//   - Variables ARE still cleared (`exist('x') == 0`).
//   - Same-chunk use: `clear all; g = inline(...)` in one script — the
//     book convention (every corpus example starts with `clear all`).
// MATLAB-probed reference values: f(3)=6, h(2)=0.238651 for
// impulse([1],[1 3 2],0:.5:2), exist('x')=0 after clear all.
//
// The inline-classdef cases run on BOTH backends since the TW ctor
// varargin fix (bugs/closed/core/treewalker-inline-classdef-ctor).

#include <cmath>

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

namespace {

class ClearSemanticsTest : public ::testing::TestWithParam<numkit::Engine::Backend> {
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.setBackend(GetParam()); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// m-source functions survive `clear all`, INCLUDING a call in the same
// chunk right after the clear (the every-book-script form).
TEST_P(ClearSemanticsTest, ClearAllKeepsBuiltinMSources)
{
    eval("clear all; h = impulse([1], [1 3 2], 0:.5:2);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(h)"), 5.0);
    EXPECT_NEAR(evalScalar("h(2)"), 0.238651, 1e-6);  // MATLAB R2025b probe
}

// ode45 is a heavier m-source registration (wraps the C++ solver with a
// pausable body) — it must survive `clear all` too. (Tolerance is the
// solver's own accuracy, not the replay's.)
TEST_P(ClearSemanticsTest, ClearAllKeepsOdeWrapper)
{
    eval("clear all; [t, y] = ode45(@(t,y) -y, [0 1], 1);");
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(-1.0), 1e-4);
}

// clear functions keeps the startup m-source functions (MATLAB reloads
// them from disk; we re-register).
TEST_P(ClearSemanticsTest, ClearFunctionsKeepsBuiltinMSources)
{
    eval("clear functions; h2 = impulse([1], [1 2], 0:1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(h2)"), 2.0);
}

// The wipe still does its primary job: variables are gone after clear all.
TEST_P(ClearSemanticsTest, ClearAllClearsVariables)
{
    eval("x = 42; y = 'str'; clear all;");
    EXPECT_DOUBLE_EQ(evalScalar("exist('x')"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("exist('y')"), 0.0);
}

// clear all / clear classes keep the startup CLASSDEF callable,
// including same-chunk use. Dual-engine since the TW ctor varargin fix
// (bugs/closed/core/treewalker-inline-classdef-ctor).
TEST_P(ClearSemanticsTest, ClearAllKeepsInlineClassdef)
{
    eval("clear all; g = inline('2*t', 't');");
    EXPECT_EQ(eval("class(g);").toString(), "inline");
    EXPECT_DOUBLE_EQ(evalScalar("g(3)"), 6.0);
    eval("clear classes; g2 = inline('t', 't');");
    EXPECT_DOUBLE_EQ(evalScalar("g2(7)"), 7.0);
}

INSTANTIATE_TEST_SUITE_P(Backends, ClearSemanticsTest,
                         ::testing::Values(numkit::Engine::Backend::VM,
                                           numkit::Engine::Backend::TreeWalker));

} // namespace
