// libs/builtin/tests/calculus_test.cpp
//
// Numerical calculus: gradient, cumtrapz.

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class CalculusTest : public DualEngineTest
{};

// ── gradient: 1D ───────────────────────────────────────────────

TEST_P(CalculusTest, GradientLinearVectorIsConstant)
{
    // f(x) = 2x → gradient = 2 everywhere.
    eval("g = gradient([0 2 4 6 8 10]);");
    auto *g = getVarPtr("g");
    EXPECT_EQ(g->numel(), 6u);
    for (size_t i = 0; i < 6; ++i)
        EXPECT_DOUBLE_EQ(g->doubleData()[i], 2.0);
}

TEST_P(CalculusTest, GradientCentralDifferenceInteriorEndpointsOneSided)
{
    // f = [1 4 9 16 25] (squares)
    // Endpoints: forward/backward = 3, 9.
    // Interior central: (9-1)/2=4, (16-4)/2=6, (25-9)/2=8.
    eval("g = gradient([1 4 9 16 25]);");
    auto *g = getVarPtr("g");
    EXPECT_DOUBLE_EQ(g->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(g->doubleData()[1], 4.0);
    EXPECT_DOUBLE_EQ(g->doubleData()[2], 6.0);
    EXPECT_DOUBLE_EQ(g->doubleData()[3], 8.0);
    EXPECT_DOUBLE_EQ(g->doubleData()[4], 9.0);
}

TEST_P(CalculusTest, GradientWithSpacing)
{
    // gradient([0 1 4 9 16], 2): h=2 → divide central diffs by 4, endpoints by 2.
    eval("g = gradient([0 1 4 9 16], 2);");
    auto *g = getVarPtr("g");
    // Endpoint forward: (1-0)/2 = 0.5
    EXPECT_DOUBLE_EQ(g->doubleData()[0], 0.5);
    // Central (i=1): (4-0)/4 = 1
    EXPECT_DOUBLE_EQ(g->doubleData()[1], 1.0);
    // Endpoint backward: (16-9)/2 = 3.5
    EXPECT_DOUBLE_EQ(g->doubleData()[4], 3.5);
}

TEST_P(CalculusTest, GradientLengthOneIsZero)
{
    eval("g = gradient([7]);");
    auto *g = getVarPtr("g");
    EXPECT_EQ(g->numel(), 1u);
    EXPECT_DOUBLE_EQ(g->doubleData()[0], 0.0);
}

TEST_P(CalculusTest, GradientColumnVector)
{
    eval("g = gradient([1; 4; 9; 16]);");
    auto *g = getVarPtr("g");
    EXPECT_EQ(rows(*g), 4u);
    EXPECT_EQ(cols(*g), 1u);
    EXPECT_DOUBLE_EQ(g->doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(g->doubleData()[1], 4.0);
    EXPECT_DOUBLE_EQ(g->doubleData()[2], 6.0);
    EXPECT_DOUBLE_EQ(g->doubleData()[3], 7.0);
}

// ── gradient: 2D ───────────────────────────────────────────────

TEST_P(CalculusTest, GradientMatrixSingleOutputIsXDirection)
{
    // M = [1 2 4 7; 8 9 11 14] → gradient along columns (dim-2)
    // Row 0: forward 1, central 1.5, central 2.5, backward 3.
    // Row 1: forward 1, central 1.5, central 2.5, backward 3.
    eval("g = gradient([1 2 4 7; 8 9 11 14]);");
    auto *g = getVarPtr("g");
    EXPECT_EQ(rows(*g), 2u);
    EXPECT_EQ(cols(*g), 4u);
    const double expected[2][4] = {
        {1.0, 1.5, 2.5, 3.0},
        {1.0, 1.5, 2.5, 3.0},
    };
    for (size_t r = 0; r < 2; ++r)
        for (size_t c = 0; c < 4; ++c)
            EXPECT_DOUBLE_EQ((*g)(r, c), expected[r][c])
                << "at (" << r << "," << c << ")";
}

TEST_P(CalculusTest, GradientTwoOutputsXY)
{
    eval("[fx, fy] = gradient([1 2 3; 4 5 6; 7 8 9]);");
    auto *fx = getVarPtr("fx");
    auto *fy = getVarPtr("fy");
    // fx (along columns): each row is [1, 1, 1].
    for (size_t r = 0; r < 3; ++r)
        for (size_t c = 0; c < 3; ++c)
            EXPECT_DOUBLE_EQ((*fx)(r, c), 1.0)
                << "fx at (" << r << "," << c << ")";
    // fy (along rows): each column is [3, 3, 3].
    for (size_t r = 0; r < 3; ++r)
        for (size_t c = 0; c < 3; ++c)
            EXPECT_DOUBLE_EQ((*fy)(r, c), 3.0)
                << "fy at (" << r << "," << c << ")";
}

TEST_P(CalculusTest, GradientMatrixWithSeparateSpacings)
{
    // hx=2, hy=3 — divide x-dir diffs by 2, y-dir diffs by 3.
    eval("[fx, fy] = gradient([1 2 3; 4 5 6; 7 8 9], 2, 3);");
    auto *fx = getVarPtr("fx");
    auto *fy = getVarPtr("fy");
    EXPECT_DOUBLE_EQ((*fx)(0, 0), 0.5);
    EXPECT_DOUBLE_EQ((*fy)(0, 0), 1.0);
}

TEST_P(CalculusTest, GradientBadSpacingThrows)
{
    EXPECT_THROW(eval("g = gradient([1 2 3], 0);"),  std::exception);
    EXPECT_THROW(eval("g = gradient([1 2 3], -1);"), std::exception);
}

TEST_P(CalculusTest, Gradient3DInputThrows)
{
    eval("A = zeros(2, 2, 2);");
    EXPECT_THROW(eval("g = gradient(A);"), std::exception);
}

TEST_P(CalculusTest, GradientComplexThrows)
{
    EXPECT_THROW(eval("g = gradient([1+2i, 3+0i, 5-1i]);"), std::exception);
}

// ── cumtrapz ───────────────────────────────────────────────────

TEST_P(CalculusTest, CumtrapzUnitSpacingMatchesFormula)
{
    // y = [1 2 3 4]: cum[i] = sum_{k=1..i} 0.5*(y[k-1]+y[k])
    // [0, 1.5, 4.0, 7.5]
    eval("c = cumtrapz([1 2 3 4]);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(c->numel(), 4u);
    EXPECT_DOUBLE_EQ(c->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[1], 1.5);
    EXPECT_DOUBLE_EQ(c->doubleData()[2], 4.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[3], 7.5);
}

TEST_P(CalculusTest, CumtrapzWithExplicitX)
{
    // x = [0 0.5 1 1.5], y = [1 1 1 1]: trap area = 0, 0.5, 1, 1.5.
    eval("c = cumtrapz([0 0.5 1 1.5], [1 1 1 1]);");
    auto *c = getVarPtr("c");
    EXPECT_DOUBLE_EQ(c->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[1], 0.5);
    EXPECT_DOUBLE_EQ(c->doubleData()[2], 1.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[3], 1.5);
}

TEST_P(CalculusTest, CumtrapzPreservesShape)
{
    // Column vector.
    eval("c = cumtrapz([1; 2; 3; 4]);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(rows(*c), 4u);
    EXPECT_EQ(cols(*c), 1u);
}

TEST_P(CalculusTest, CumtrapzApproximatesLinearIntegral)
{
    // y = x for x ∈ [0, 1] sampled at 100 points → cumtrapz ≈ 0.5·x².
    // Final value should be ~ 0.5.
    eval("x = linspace(0, 1, 101);"
         "y = x;"
         "c = cumtrapz(x, y);");
    EXPECT_NEAR(evalScalar("c(101);"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("c(51);"),  0.125, 1e-12);  // 0.5 * 0.5²
}

TEST_P(CalculusTest, CumtrapzLengthMismatchThrows)
{
    EXPECT_THROW(eval("c = cumtrapz([1 2 3], [1 2]);"), std::exception);
}

TEST_P(CalculusTest, CumtrapzMatrixThrows)
{
    EXPECT_THROW(eval("c = cumtrapz([1 2; 3 4]);"), std::exception);
}

TEST_P(CalculusTest, CumtrapzComplexThrows)
{
    EXPECT_THROW(eval("c = cumtrapz([1+2i, 3, 5]);"), std::exception);
}

// ── fzero ──────────────────────────────────────────────────────

// fzero relies on the engine callback API which needs the TW backend
// for anonymous handles (see Engine::callFunctionHandleMulti).
#define FZERO_REQUIRE_TW()                                                  \
    do {                                                                     \
        if (GetParam() == BackendParam::VM)                                  \
            GTEST_SKIP() << "fzero: VM-side anonymous handle callback not "  \
                            "yet supported (round 11 item 27 limitation)";   \
    } while (0)

TEST_P(CalculusTest, FzeroQuadraticBracket)
{
    FZERO_REQUIRE_TW();
    // Find root of x^2 - 4 in [0, 10] → 2.
    eval("r = fzero(@(x) x.^2 - 4, [0, 10]);");
    EXPECT_NEAR(evalScalar("r;"), 2.0, 1e-10);
}

TEST_P(CalculusTest, FzeroStartFromX0)
{
    FZERO_REQUIRE_TW();
    // Same root but only an x0 hint.
    eval("r = fzero(@(x) x - sqrt(2), 1);");
    EXPECT_NEAR(evalScalar("r;"), std::sqrt(2.0), 1e-12);
}

TEST_P(CalculusTest, FzeroSineRootNearPi)
{
    FZERO_REQUIRE_TW();
    eval("r = fzero(@(x) sin(x), [3, 4]);");
    EXPECT_NEAR(evalScalar("r;"), M_PI, 1e-10);
}

TEST_P(CalculusTest, FzeroLinearWithClosure)
{
    FZERO_REQUIRE_TW();
    // f(x) = x - k where k is captured.
    eval("k = 7.5;"
         "r = fzero(@(x) x - k, 0);");
    EXPECT_NEAR(evalScalar("r;"), 7.5, 1e-12);
}

TEST_P(CalculusTest, FzeroBuiltinHandle)
{
    // @cos has a root near pi/2. Built-in handle (no anonymous closure)
    // works on both backends.
    eval("r = fzero(@cos, [1, 2]);");
    EXPECT_NEAR(evalScalar("r;"), M_PI / 2.0, 1e-10);
}

TEST_P(CalculusTest, FzeroNoSignChangeThrows)
{
    FZERO_REQUIRE_TW();
    EXPECT_THROW(eval("r = fzero(@(x) x.^2 + 1, [-1, 1]);"), std::exception);
}

TEST_P(CalculusTest, FzeroBadIntervalThrows)
{
    FZERO_REQUIRE_TW();
    // a >= b is invalid.
    EXPECT_THROW(eval("r = fzero(@(x) x, [5, 1]);"), std::exception);
}

TEST_P(CalculusTest, FzeroNonHandleThrows)
{
    EXPECT_THROW(eval("r = fzero('not a handle', 1);"), std::exception);
}

INSTANTIATE_DUAL(CalculusTest);
