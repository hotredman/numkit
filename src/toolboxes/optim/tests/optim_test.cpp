// toolboxes/optim/tests/optim_test.cpp
//
// Offline regression guard for the optim local solvers — fzero (bracket +
// initial-guess forms), fminbnd, fminsearch — plus their multi-output
// [x, fval, exitflag] shape and the no-sign-change error branch. These
// exercise the internal findBracket / brent / brentMin / nelderMead paths
// that previously had zero gtest coverage (parity-spec only).

#include "dual_engine_fixture.hpp"

#include <numkit/optim/local/fzero.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <cmath>
#include <memory_resource>

using namespace m_test;

class OptimTest : public DualEngineTest
{};

// ── fzero: bracket form (Brent on [a, b]) ───────────────────

TEST_P(OptimTest, FzeroBracketEvenRoot)
{
    eval("[x, fval, ef] = fzero(@(x) x.^2 - 4, [0 10]);");
    EXPECT_NEAR(evalScalar("x"), 2.0, 1e-9);
    EXPECT_NEAR(evalScalar("fval"), 0.0, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("ef"), 1.0);
}

TEST_P(OptimTest, FzeroBracketLinear)
{
    eval("x = fzero(@(x) 2.*x - 7, [0 10]);");
    EXPECT_NEAR(evalScalar("x"), 3.5, 1e-9);
}

// ── fzero: initial-guess form (outward bracket expansion) ───

TEST_P(OptimTest, FzeroInitialGuessExpandsRight)
{
    // f(1) = -3 < 0; the only positive-side root is at x = 2.
    eval("x = fzero(@(x) x.^2 - 4, 1);");
    EXPECT_NEAR(evalScalar("x"), 2.0, 1e-6);
    EXPECT_NEAR(evalScalar("x.^2 - 4"), 0.0, 1e-8);  // residual at the root
}

TEST_P(OptimTest, FzeroInitialGuessExpandsLeft)
{
    eval("x = fzero(@(x) x.^2 - 4, -1);");
    EXPECT_NEAR(evalScalar("x"), -2.0, 1e-6);
}

TEST_P(OptimTest, FzeroNoSignChangeThrows)
{
    // x^2 + 1 > 0 everywhere → no bracket → error.
    EXPECT_ANY_THROW(eval("fzero(@(x) x.^2 + 1, [0 10]);"));
}

// ── fminbnd: bounded scalar minimisation (Brent golden + parabolic) ─

TEST_P(OptimTest, FminbndParabola)
{
    eval("[x, fval, ef] = fminbnd(@(x) (x-3).^2 + 1, 0, 10);");
    EXPECT_NEAR(evalScalar("x"), 3.0, 1e-4);
    EXPECT_NEAR(evalScalar("fval"), 1.0, 1e-6);
    EXPECT_DOUBLE_EQ(evalScalar("ef"), 1.0);
}

TEST_P(OptimTest, FminbndNegativeRegion)
{
    eval("x = fminbnd(@(x) (x+2).^2, -10, 0);");
    EXPECT_NEAR(evalScalar("x"), -2.0, 1e-4);
}

// ── fminsearch: Nelder-Mead simplex ─────────────────────────

TEST_P(OptimTest, Fminsearch2DBowl)
{
    eval("[x, fval] = fminsearch(@(v) (v(1)-1).^2 + (v(2)-2).^2, [0 0]);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-3);
    EXPECT_NEAR(evalScalar("x(2)"), 2.0, 1e-3);
    EXPECT_NEAR(evalScalar("fval"), 0.0, 1e-6);
}

TEST_P(OptimTest, Fminsearch1D)
{
    eval("x = fminsearch(@(v) (v-5).^2, 0);");
    EXPECT_NEAR(evalScalar("x"), 5.0, 1e-3);
}

TEST_P(OptimTest, Fminsearch3DBowl)
{
    eval("x = fminsearch(@(v) (v(1)-1)^2 + (v(2)-2)^2 + (v(3)-3)^2, [0 0 0]);");
    EXPECT_NEAR(evalScalar("x(1)"), 1.0, 1e-2);
    EXPECT_NEAR(evalScalar("x(2)"), 2.0, 1e-2);
    EXPECT_NEAR(evalScalar("x(3)"), 3.0, 1e-2);
}

INSTANTIATE_DUAL(OptimTest);

// ── Direct C++ compute API (numkit::optim::*) ───────────────
// Script-level fzero/fminsearch route through the pausable embedded-.m
// wrapper, so the engine-free Brent / bracket-expansion / Nelder-Mead code
// in fzero.cpp is only reached by calling the C++ API directly (which is the
// public, Engine-free surface the library promises). These tests cover that
// path. The objective is a numkit::FnHandle: args[0] is the evaluation point
// (scalar for fzero/fminbnd, a 1xN row for fminsearch); outs[0] takes the
// scalar result, honouring the caller's memory resource.

class OptimApiTest : public ::testing::Test {};

namespace {
// f(x) = x^2 - 4  → roots ±2.
void quadMinus4(numkit::Span<const numkit::Value> a, numkit::Span<numkit::Value> o,
                std::pmr::memory_resource *mr)
{
    const double x = a[0].toScalar();
    o[0] = numkit::Value::scalar(x * x - 4.0, mr);
}
} // namespace

TEST_F(OptimApiTest, FzeroBracketDirect)  // brent on [a,b]
{
    EXPECT_NEAR(numkit::optim::fzero(quadMinus4, 0.0, 10.0).toScalar(), 2.0, 1e-9);
}

TEST_F(OptimApiTest, FzeroInitialGuessRightDirect)  // findBracket expands right
{
    EXPECT_NEAR(numkit::optim::fzero(quadMinus4, 1.0).toScalar(), 2.0, 1e-6);
}

TEST_F(OptimApiTest, FzeroInitialGuessLeftDirect)  // findBracket expands left
{
    EXPECT_NEAR(numkit::optim::fzero(quadMinus4, -1.0).toScalar(), -2.0, 1e-6);
}

TEST_F(OptimApiTest, FzeroNoSignChangeThrowsDirect)
{
    auto positive = [](numkit::Span<const numkit::Value> a, numkit::Span<numkit::Value> o,
                       std::pmr::memory_resource *mr) {
        const double x = a[0].toScalar();
        o[0] = numkit::Value::scalar(x * x + 1.0, mr);  // > 0 everywhere
    };
    EXPECT_ANY_THROW(numkit::optim::fzero(positive, 0.0, 10.0));
}

TEST_F(OptimApiTest, FminbndDirect)  // brentMin
{
    auto parab = [](numkit::Span<const numkit::Value> a, numkit::Span<numkit::Value> o,
                    std::pmr::memory_resource *mr) {
        const double x = a[0].toScalar();
        o[0] = numkit::Value::scalar((x - 3.0) * (x - 3.0) + 1.0, mr);
    };
    EXPECT_NEAR(numkit::optim::fminbnd(parab, 0.0, 10.0, 1e-6).toScalar(), 3.0, 1e-4);
}

TEST_F(OptimApiTest, FminsearchBowlDirect)  // nelderMead, n=2
{
    auto bowl = [](numkit::Span<const numkit::Value> a, numkit::Span<numkit::Value> o,
                   std::pmr::memory_resource *mr) {
        const double *v = a[0].doubleData();
        o[0] = numkit::Value::scalar((v[0] - 1.0) * (v[0] - 1.0) +
                                     (v[1] - 2.0) * (v[1] - 2.0), mr);
    };
    const double x0[] = {0.0, 0.0};
    numkit::Value r = numkit::optim::fminsearch(bowl, numkit::Span<const double>(x0, 2), 1e-8);
    ASSERT_EQ(r.numel(), 2u);
    EXPECT_NEAR(r.doubleData()[0], 1.0, 1e-3);
    EXPECT_NEAR(r.doubleData()[1], 2.0, 1e-3);
}

TEST_F(OptimApiTest, Fminsearch1DDirect)  // nelderMead, n=1
{
    auto sq = [](numkit::Span<const numkit::Value> a, numkit::Span<numkit::Value> o,
                 std::pmr::memory_resource *mr) {
        const double x = a[0].doubleData()[0];
        o[0] = numkit::Value::scalar((x - 5.0) * (x - 5.0), mr);
    };
    const double x0[] = {0.0};
    EXPECT_NEAR(
        numkit::optim::fminsearch(sq, numkit::Span<const double>(x0, 1), 1e-8).doubleData()[0],
        5.0, 1e-3);
}
