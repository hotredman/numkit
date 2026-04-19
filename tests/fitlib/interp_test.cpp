// tests/interp_test.cpp

#include "MEngine.hpp"
#include "MStdLibrary.hpp"
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit::m::m;

class InterpTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { StdLibrary::install(engine); }
    MValue eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ============================================================
// interp1 — linear (default)
// ============================================================

TEST_F(InterpTest, LinearMidpoint)
{
    EXPECT_NEAR(evalScalar("interp1([0 1], [0 10], 0.5)"), 5.0, 1e-10);
}

TEST_F(InterpTest, LinearAtNodes)
{
    eval("yq = interp1([1 2 3], [10 20 30], [1 2 3]);");
    EXPECT_NEAR(evalScalar("yq(1)"), 10.0, 1e-10);
    EXPECT_NEAR(evalScalar("yq(2)"), 20.0, 1e-10);
    EXPECT_NEAR(evalScalar("yq(3)"), 30.0, 1e-10);
}

TEST_F(InterpTest, LinearBetween)
{
    eval("yq = interp1([0 1 2], [0 1 4], 1.5);");
    // Between (1,1) and (2,4): 1 + 0.5*3 = 2.5
    EXPECT_NEAR(evalScalar("yq"), 2.5, 1e-10);
}

TEST_F(InterpTest, LinearMultipleQuery)
{
    eval("yq = interp1([0 1 2], [0 2 8], [0.5 1.5]);");
    EXPECT_EQ(eval("yq").numel(), 2u);
    EXPECT_NEAR(evalScalar("yq(1)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("yq(2)"), 5.0, 1e-10);
}

// ============================================================
// interp1 — nearest
// ============================================================

TEST_F(InterpTest, NearestSnapsToCloser)
{
    eval("yq = interp1([0 1 2], [10 20 30], 0.3, 'nearest');");
    EXPECT_NEAR(evalScalar("yq"), 10.0, 1e-10);
}

TEST_F(InterpTest, NearestSnapsToRight)
{
    eval("yq = interp1([0 1 2], [10 20 30], 0.7, 'nearest');");
    EXPECT_NEAR(evalScalar("yq"), 20.0, 1e-10);
}

TEST_F(InterpTest, NearestAtNode)
{
    eval("yq = interp1([0 1 2], [10 20 30], 1, 'nearest');");
    EXPECT_NEAR(evalScalar("yq"), 20.0, 1e-10);
}

// ============================================================
// interp1 — spline
// ============================================================

TEST_F(InterpTest, SplineAtNodes)
{
    eval("yq = interp1([0 1 2 3], [0 1 4 9], [0 1 2 3], 'spline');");
    EXPECT_NEAR(evalScalar("yq(1)"), 0.0, 1e-8);
    EXPECT_NEAR(evalScalar("yq(2)"), 1.0, 1e-8);
    EXPECT_NEAR(evalScalar("yq(3)"), 4.0, 1e-8);
    EXPECT_NEAR(evalScalar("yq(4)"), 9.0, 1e-8);
}

TEST_F(InterpTest, SplineSmoothBetween)
{
    // Spline through y=x^2 should give good approximation
    eval("x = [0 1 2 3 4];");
    eval("y = [0 1 4 9 16];");
    eval("yq = interp1(x, y, 1.5, 'spline');");
    EXPECT_NEAR(evalScalar("yq"), 2.25, 0.3); // x^2 at 1.5 = 2.25
}

TEST_F(InterpTest, SplineFunction)
{
    // spline(x, y, xq) shortcut
    eval("yq = spline([0 1 2 3], [0 1 4 9], 1.5);");
    EXPECT_NEAR(evalScalar("yq"), 2.25, 0.3);
}

// ============================================================
// interp1 — pchip
// ============================================================

TEST_F(InterpTest, PchipAtNodes)
{
    eval("yq = interp1([0 1 2 3], [0 1 0 1], [0 1 2 3], 'pchip');");
    EXPECT_NEAR(evalScalar("yq(1)"), 0.0, 1e-8);
    EXPECT_NEAR(evalScalar("yq(2)"), 1.0, 1e-8);
    EXPECT_NEAR(evalScalar("yq(3)"), 0.0, 1e-8);
    EXPECT_NEAR(evalScalar("yq(4)"), 1.0, 1e-8);
}

TEST_F(InterpTest, PchipMonotone)
{
    // PCHIP preserves monotonicity between nodes
    eval("x = [0 1 2 3]; y = [0 1 1 2];");
    eval("xq = linspace(1, 2, 10);");
    eval("yq = interp1(x, y, xq, 'pchip');");
    // Between x=1 and x=2, y should stay near 1 (monotone flat)
    EXPECT_NEAR(evalScalar("min(yq)"), 1.0, 0.05);
    EXPECT_NEAR(evalScalar("max(yq)"), 1.0, 0.05);
}

TEST_F(InterpTest, PchipFunction)
{
    eval("yq = pchip([0 1 2 3], [0 1 4 9], 1.5);");
    EXPECT_NEAR(evalScalar("yq"), 2.25, 0.5);
}

// ============================================================
// polyfit
// ============================================================

TEST_F(InterpTest, PolyfitLinear)
{
    // Fit line to y = 2x + 1
    eval("p = polyfit([0 1 2 3], [1 3 5 7], 1);");
    EXPECT_NEAR(evalScalar("p(1)"), 2.0, 1e-8);  // slope
    EXPECT_NEAR(evalScalar("p(2)"), 1.0, 1e-8);  // intercept
}

TEST_F(InterpTest, PolyfitQuadratic)
{
    // Fit parabola to y = x^2
    eval("p = polyfit([0 1 2 3 4], [0 1 4 9 16], 2);");
    EXPECT_NEAR(evalScalar("p(1)"), 1.0, 1e-6);  // x^2 coeff
    EXPECT_NEAR(evalScalar("p(2)"), 0.0, 1e-6);  // x coeff
    EXPECT_NEAR(evalScalar("p(3)"), 0.0, 1e-6);  // constant
}

TEST_F(InterpTest, PolyfitCubic)
{
    // Fit y = x^3
    eval("x = [-2 -1 0 1 2 3];");
    eval("y = x .^ 3;");
    eval("p = polyfit(x, y, 3);");
    EXPECT_NEAR(evalScalar("p(1)"), 1.0, 1e-4);  // x^3
    EXPECT_NEAR(evalScalar("p(2)"), 0.0, 1e-4);  // x^2
    EXPECT_NEAR(evalScalar("p(3)"), 0.0, 1e-4);  // x
    EXPECT_NEAR(evalScalar("p(4)"), 0.0, 1e-4);  // const
}

TEST_F(InterpTest, PolyfitConstant)
{
    // Fit degree 0 (constant)
    eval("p = polyfit([1 2 3 4], [5 5 5 5], 0);");
    EXPECT_NEAR(evalScalar("p(1)"), 5.0, 1e-10);
}

TEST_F(InterpTest, PolyfitOutputLength)
{
    // Degree n → n+1 coefficients
    EXPECT_EQ(eval("polyfit([1 2 3], [1 2 3], 2)").numel(), 3u);
}

// ============================================================
// polyval
// ============================================================

TEST_F(InterpTest, PolyvalLinear)
{
    // p = [2 1] → 2x + 1
    EXPECT_NEAR(evalScalar("polyval([2 1], 3)"), 7.0, 1e-10);
}

TEST_F(InterpTest, PolyvalQuadratic)
{
    // p = [1 0 0] → x^2
    EXPECT_NEAR(evalScalar("polyval([1 0 0], 5)"), 25.0, 1e-10);
}

TEST_F(InterpTest, PolyvalArray)
{
    eval("y = polyval([1 0], [1 2 3 4]);");
    EXPECT_EQ(eval("y").numel(), 4u);
    EXPECT_NEAR(evalScalar("y(1)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("y(4)"), 4.0, 1e-10);
}

TEST_F(InterpTest, PolyvalConstant)
{
    EXPECT_NEAR(evalScalar("polyval([7], 100)"), 7.0, 1e-10);
}

TEST_F(InterpTest, PolyfitPolyvalRoundtrip)
{
    // Fit and evaluate should recover original data
    eval("x = [0 1 2 3 4]; y = [1 3 7 13 21];");
    eval("p = polyfit(x, y, 3);");
    eval("yhat = polyval(p, x);");
    for (int i = 1; i <= 5; ++i) {
        std::string yi = "y(" + std::to_string(i) + ")";
        std::string yhati = "yhat(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(yhati), evalScalar(yi), 0.1);
    }
}

// ============================================================
// trapz
// ============================================================

TEST_F(InterpTest, TrapzUnitSpacing)
{
    // trapz([1 2 3]) = 0.5*(1+2) + 0.5*(2+3) = 4
    EXPECT_NEAR(evalScalar("trapz([1 2 3])"), 4.0, 1e-10);
}

TEST_F(InterpTest, TrapzConstant)
{
    // Integral of 5 over [0, 4] with 5 points = 5*4 = 20
    EXPECT_NEAR(evalScalar("trapz([0 1 2 3 4], 5*ones(1,5))"), 20.0, 1e-10);
}

TEST_F(InterpTest, TrapzLinear)
{
    // Integral of y=x from 0 to 1 = 0.5
    eval("x = linspace(0, 1, 1000);");
    double result = evalScalar("trapz(x, x)");
    EXPECT_NEAR(result, 0.5, 1e-4);
}

TEST_F(InterpTest, TrapzSine)
{
    // Integral of sin(x) from 0 to pi = 2
    eval("x = linspace(0, pi, 10000);");
    eval("y = sin(x);");
    EXPECT_NEAR(evalScalar("trapz(x, y)"), 2.0, 1e-4);
}

TEST_F(InterpTest, TrapzUnevenSpacing)
{
    // x = [0 1 3], y = [0 1 1] → 0.5*(0+1)*1 + 0.5*(1+1)*2 = 0.5 + 2 = 2.5
    EXPECT_NEAR(evalScalar("trapz([0 1 3], [0 1 1])"), 2.5, 1e-10);
}
