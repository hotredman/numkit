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

// gradient now supports N-D arrays (was: threw). Single output = the dim-2 (x)
// gradient; [gx,gy,gz] adds dim-1 (y) and dim-3 (z). MATLAB gz(1,1,1)=4.
// (bugs/builtin/gradient-3d.md FIXED 2026-06-05.)
TEST_P(CalculusTest, Gradient3DInput)
{
    eval("A = reshape(1:8, 2, 2, 2);");
    eval("g = gradient(A);");
    EXPECT_NEAR(evalScalar("g(1,1,1)"), 2.0, 1e-12);
    eval("[gx, gy, gz] = gradient(A);");
    EXPECT_NEAR(evalScalar("gy(1,1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("gz(1,1,1)"), 4.0, 1e-12);
}

// gradient accepts complex as of 2026-06-05 (was: threw). Real + imaginary
// parts gradiented separately. MATLAB: gradient([1+2i 3 5-1i]) = [2-2i 2-1.5i 2-1i].
TEST_P(CalculusTest, GradientComplexOk)
{
    eval("g = gradient([1+2i, 3+0i, 5-1i]);");
    EXPECT_NEAR(evalScalar("real(g(2));"),  2.0,  1e-12);
    EXPECT_NEAR(evalScalar("imag(g(2));"), -1.5,  1e-12);
    EXPECT_NEAR(evalScalar("imag(g(1));"), -2.0,  1e-12);
    EXPECT_NEAR(evalScalar("imag(g(3));"), -1.0,  1e-12);
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

TEST_P(CalculusTest, CumtrapzMatrixIntegratesColumns)
{
    // Matrix input: integrate down each column with unit spacing.
    //   col 1 = [1; 3]  → [0; 0.5*(1+3)*1] = [0; 2]
    //   col 2 = [2; 4]  → [0; 0.5*(2+4)*1] = [0; 3]
    eval("c = cumtrapz([1 2; 3 4]);");
    auto *c = engine.getVariable("c");
    ASSERT_NE(c, nullptr);
    EXPECT_DOUBLE_EQ((*c)(0, 0), 0.0);
    EXPECT_DOUBLE_EQ((*c)(1, 0), 2.0);
    EXPECT_DOUBLE_EQ((*c)(0, 1), 0.0);
    EXPECT_DOUBLE_EQ((*c)(1, 1), 3.0);
}

TEST_P(CalculusTest, CumtrapzDim1MatchesColumnDefault)
{
    // cumtrapz(A, 1) == cumtrapz(A): integrate down columns.
    eval("c = cumtrapz([1 2; 3 4], 1);");
    auto *c = engine.getVariable("c");
    ASSERT_NE(c, nullptr);
    EXPECT_DOUBLE_EQ((*c)(0, 0), 0.0);
    EXPECT_DOUBLE_EQ((*c)(1, 0), 2.0);
    EXPECT_DOUBLE_EQ((*c)(0, 1), 0.0);
    EXPECT_DOUBLE_EQ((*c)(1, 1), 3.0);
}

TEST_P(CalculusTest, CumtrapzDim2IntegratesRows)
{
    // cumtrapz(A, 2): integrate along each row with unit spacing.
    //   row 1 = [1 2] → [0, 0.5*(1+2)] = [0, 1.5]
    //   row 2 = [3 4] → [0, 0.5*(3+4)] = [0, 3.5]
    eval("c = cumtrapz([1 2; 3 4], 2);");
    auto *c = engine.getVariable("c");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(rows(*c), 2u);
    EXPECT_EQ(cols(*c), 2u);
    EXPECT_DOUBLE_EQ((*c)(0, 0), 0.0);
    EXPECT_DOUBLE_EQ((*c)(0, 1), 1.5);
    EXPECT_DOUBLE_EQ((*c)(1, 0), 0.0);
    EXPECT_DOUBLE_EQ((*c)(1, 1), 3.5);
}

TEST_P(CalculusTest, CumtrapzVectorDimNoOpAlongSingleton)
{
    // Row vector along dim 1 (singleton) → all zeros, same shape.
    eval("c = cumtrapz([1 2 3 4], 1);");
    auto *c = getVarPtr("c");
    EXPECT_EQ(c->numel(), 4u);
    EXPECT_DOUBLE_EQ(c->doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[1], 0.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[2], 0.0);
    EXPECT_DOUBLE_EQ(c->doubleData()[3], 0.0);
}

TEST_P(CalculusTest, CumtrapzXYDim2RowWise)
{
    // cumtrapz(X, Y, 2): X is a coordinate vector of length size(Y,2),
    // broadcast across rows (MATLAB form).
    //   X = [0 1 2], Y row 1 = [3 4 5] → [0, 3.5, 8]; row 2 = [1 1 1] → [0, 1, 2]
    eval("c = cumtrapz([0 1 2], [3 4 5; 1 1 1], 2);");
    auto *c = engine.getVariable("c");
    ASSERT_NE(c, nullptr);
    EXPECT_DOUBLE_EQ((*c)(0, 0), 0.0);
    EXPECT_DOUBLE_EQ((*c)(0, 1), 3.5);
    EXPECT_DOUBLE_EQ((*c)(0, 2), 8.0);
    EXPECT_DOUBLE_EQ((*c)(1, 0), 0.0);
    EXPECT_DOUBLE_EQ((*c)(1, 1), 1.0);
    EXPECT_DOUBLE_EQ((*c)(1, 2), 2.0);
}

// cumtrapz accepts complex y as of 2026-06-05 (was: threw). The cumulative
// trapezoid runs over Complex storage. MATLAB: cumtrapz([1+2i 3 5]) = [0 2+1i 6+1i].
TEST_P(CalculusTest, CumtrapzComplexOk)
{
    eval("c = cumtrapz([1+2i, 3, 5]);");
    EXPECT_NEAR(evalScalar("real(c(2));"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(2));"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(c(3));"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(c(3));"), 1.0, 1e-12);
}

// ── fzero ──────────────────────────────────────────────────────

TEST_P(CalculusTest, FzeroQuadraticBracket)
{
    // Find root of x^2 - 4 in [0, 10] → 2.
    eval("r = fzero(@(x) x.^2 - 4, [0, 10]);");
    EXPECT_NEAR(evalScalar("r;"), 2.0, 1e-10);
}

TEST_P(CalculusTest, FzeroStartFromX0)
{
    // Same root but only an x0 hint.
    eval("r = fzero(@(x) x - sqrt(2), 1);");
    EXPECT_NEAR(evalScalar("r;"), std::sqrt(2.0), 1e-12);
}

TEST_P(CalculusTest, FzeroSineRootNearPi)
{
    eval("r = fzero(@(x) sin(x), [3, 4]);");
    EXPECT_NEAR(evalScalar("r;"), M_PI, 1e-10);
}

TEST_P(CalculusTest, FzeroLinearWithClosure)
{
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
    EXPECT_THROW(eval("r = fzero(@(x) x.^2 + 1, [-1, 1]);"), std::exception);
}

TEST_P(CalculusTest, FzeroBadIntervalThrows)
{
    // a >= b is invalid.
    EXPECT_THROW(eval("r = fzero(@(x) x, [5, 1]);"), std::exception);
}

TEST_P(CalculusTest, FzeroNonHandleThrows)
{
    EXPECT_THROW(eval("r = fzero('not a handle', 1);"), std::exception);
}

// ── integral ───────────────────────────────────────────────────

TEST_P(CalculusTest, IntegralPolynomial)
{
    // ∫_0^1 x^2 dx = 1/3.
    eval("r = integral(@(x) x.^2, 0, 1);");
    EXPECT_NEAR(evalScalar("r;"), 1.0 / 3.0, 1e-10);
}

TEST_P(CalculusTest, IntegralSinPi)
{
    eval("r = integral(@(x) sin(x), 0, pi);");
    EXPECT_NEAR(evalScalar("r;"), 2.0, 1e-10);
}

TEST_P(CalculusTest, IntegralBuiltinHandleCos)
{
    // @cos works on both backends.
    eval("r = integral(@cos, 0, pi/2);");
    EXPECT_NEAR(evalScalar("r;"), 1.0, 1e-10);
}

TEST_P(CalculusTest, IntegralBoundsReversed)
{
    // ∫_1^0 x dx = -1/2.
    eval("r = integral(@(x) x, 1, 0);");
    EXPECT_NEAR(evalScalar("r;"), -0.5, 1e-12);
}

TEST_P(CalculusTest, IntegralEqualBoundsZero)
{
    eval("r = integral(@cos, 1, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("r;"), 0.0);
}

TEST_P(CalculusTest, IntegralCustomTolerance)
{
    // Use a loose tolerance — should still be near-exact for smooth integrand.
    eval("r = integral(@(x) exp(-x.^2), -3, 3, 'AbsTol', 1e-6);");
    // Reference: 2 * sqrt(pi) * erf(3) ≈ 1.7724528...
    EXPECT_NEAR(evalScalar("r;"), 1.77241469951526, 1e-5);
}

TEST_P(CalculusTest, IntegralBadBoundsThrows)
{
    EXPECT_THROW(eval("r = integral(@cos, NaN, 1);"), std::exception);
    EXPECT_THROW(eval("r = integral(@cos, 0, Inf);"), std::exception);
}

TEST_P(CalculusTest, IntegralBadTolThrows)
{
    EXPECT_THROW(eval("r = integral(@cos, 0, 1, 'AbsTol', -1e-6);"), std::exception);
}

TEST_P(CalculusTest, IntegralUnknownFlagThrows)
{
    EXPECT_THROW(eval("r = integral(@cos, 0, 1, 'NoSuchFlag', 1);"), std::exception);
}

TEST_P(CalculusTest, IntegralNonHandleThrows)
{
    EXPECT_THROW(eval("r = integral('not a handle', 0, 1);"), std::exception);
}

// trapz: matrix reduces per-column (was flattened), trapz(Y,dim) (a scalar
// 2nd arg is the dim, NOT y — was erroring), and trapz(X,Y[,dim]) spacing.
// vs MATLAB R2025b.
TEST_P(CalculusTest, TrapzMatrixAndDim)
{
    EXPECT_DOUBLE_EQ(evalScalar("trapz([1 4 9 16])"),          21.5);
    EXPECT_DOUBLE_EQ(evalScalar("trapz([0 1 2 3], [1 4 9 16])"), 21.5);
    // trapz(M) integrates each column (dim 1) -> 1x3 row.
    eval("tm = trapz([1 2 3; 4 5 6]);");
    EXPECT_DOUBLE_EQ(evalScalar("tm(1)"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("tm(3)"), 4.5);
    // trapz(M, 2) integrates each row -> 2x1 column.
    eval("td = trapz([1 2 3; 4 5 6], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("td(1)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("td(2)"), 10.0);
    // trapz(X, M, 2): per-row with X spacing.
    eval("tx = trapz([10 20 30], [1 2 3; 4 5 6], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("tx(1)"),  40.0);
    EXPECT_DOUBLE_EQ(evalScalar("tx(2)"), 100.0);
}

INSTANTIATE_DUAL(CalculusTest);
