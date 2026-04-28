// tests/interp_test.cpp

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit::m;

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

// ============================================================
// interp2 — bilinear / nearest
// ============================================================

TEST_F(InterpTest, Interp2ImplicitGridLinearAtVertex)
{
    // V = [10 20; 30 40] (R=2, C=2). At (Xq=1, Yq=1) value should be V(1,1) = 10.
    eval("V = [10 20; 30 40];");
    EXPECT_NEAR(evalScalar("interp2(V, 1, 1);"), 10.0, 1e-12);
    EXPECT_NEAR(evalScalar("interp2(V, 2, 1);"), 20.0, 1e-12);
    EXPECT_NEAR(evalScalar("interp2(V, 1, 2);"), 30.0, 1e-12);
    EXPECT_NEAR(evalScalar("interp2(V, 2, 2);"), 40.0, 1e-12);
}

TEST_F(InterpTest, Interp2BilinearMidpoint)
{
    // V = [0 0; 0 1] → bilinear value at (xq=1.5, yq=1.5) = 0.25.
    eval("V = [0 0; 0 1];");
    EXPECT_NEAR(evalScalar("interp2(V, 1.5, 1.5);"), 0.25, 1e-12);
}

TEST_F(InterpTest, Interp2BilinearKnownPlane)
{
    // f(x, y) = 2x + 3y → bilinear is exact.
    // Build V where V(r, c) = 2*c + 3*r (row r maps to y=r, col c maps to x=c).
    eval("V = [5 7 9; 8 10 12; 11 13 15];");  // V(r,c) = 2c+3r at 1-based
    EXPECT_NEAR(evalScalar("interp2(V, 1.5, 2.0);"), 2.0*1.5 + 3.0*2.0, 1e-12);
    EXPECT_NEAR(evalScalar("interp2(V, 2.7, 1.3);"), 2.0*2.7 + 3.0*1.3, 1e-12);
}

TEST_F(InterpTest, Interp2NearestNeighbour)
{
    eval("V = [10 20; 30 40];");
    // At (1.7, 1.2) nearest is (2, 1) → 20. At (1.4, 1.6) nearest is (1, 2) → 30.
    EXPECT_NEAR(evalScalar("interp2(V, 1.7, 1.2, 'nearest');"), 20.0, 1e-12);
    EXPECT_NEAR(evalScalar("interp2(V, 1.4, 1.6, 'nearest');"), 30.0, 1e-12);
}

TEST_F(InterpTest, Interp2OutsideGridIsNaN)
{
    eval("V = [10 20; 30 40];"
         "y = interp2(V, 0.5, 1);");
    EXPECT_TRUE(std::isnan(evalScalar("y;")));
    eval("y2 = interp2(V, 1, 3);");
    EXPECT_TRUE(std::isnan(evalScalar("y2;")));
}

TEST_F(InterpTest, Interp2ExplicitGrids)
{
    // V over X = [0 0.5 1], Y = [0 1] → V(r,c) at (xq, yq)
    eval("X = [0 0.5 1];"
         "Y = [0 1];"
         "V = [0 5 10; 10 15 20];");
    // At (xq=0.25, yq=0.5): bilinear of V over the bottom-left cell.
    // V(1,1)=0, V(1,2)=5, V(2,1)=10, V(2,2)=15. tx=(0.25-0)/0.5=0.5; ty=(0.5-0)/1=0.5.
    // → 0.25*0 + 0.25*5 + 0.25*10 + 0.25*15 = 7.5.
    EXPECT_NEAR(evalScalar("interp2(X, Y, V, 0.25, 0.5);"), 7.5, 1e-12);
}

TEST_F(InterpTest, Interp2ArrayQuery)
{
    eval("V = [0 0; 0 1];"
         "Xq = [1 1.5 2];"
         "Yq = [1 1.5 2];"
         "y = interp2(V, Xq, Yq);");
    // Diagonal of bilinear interpolation through V → [0, 0.25, 1].
    EXPECT_NEAR(evalScalar("y(1);"), 0.0,  1e-12);
    EXPECT_NEAR(evalScalar("y(2);"), 0.25, 1e-12);
    EXPECT_NEAR(evalScalar("y(3);"), 1.0,  1e-12);
}

TEST_F(InterpTest, Interp2GridSizeMismatchThrows)
{
    eval("V = [1 2; 3 4]; X = [0 1 2];");  // X length 3 but V has 2 cols
    EXPECT_THROW(eval("y = interp2(X, [0 1], V, 0.5, 0.5);"), std::exception);
}

TEST_F(InterpTest, Interp2NonMonotonicGridThrows)
{
    eval("V = [1 2; 3 4];");
    EXPECT_THROW(eval("y = interp2([1 0], [0 1], V, 0.5, 0.5);"), std::exception);
}

TEST_F(InterpTest, Interp2UnsupportedMethodThrows)
{
    eval("V = [1 2; 3 4];");
    EXPECT_THROW(eval("y = interp2(V, 1.5, 1.5, 'spline');"), std::exception);
}

TEST_F(InterpTest, Interp2ComplexThrows)
{
    EXPECT_THROW(eval("y = interp2([1+2i, 3; 4, 5], 1.5, 1.5);"), std::exception);
}

TEST_F(InterpTest, Interp2QueryShapeMismatchThrows)
{
    eval("V = [1 2; 3 4];");
    EXPECT_THROW(eval("y = interp2(V, [1 1.5], [1 1.5 2]);"), std::exception);
}

// ============================================================
// interp3 / interpn
// ============================================================

TEST_F(InterpTest, Interp3VertexMatch)
{
    // Build V = ones(2, 2, 2) * (page) — easy to verify.
    eval("V = zeros(2, 2, 2);"
         "V(:,:,1) = [1 2; 3 4];"
         "V(:,:,2) = [5 6; 7 8];");
    EXPECT_NEAR(evalScalar("interp3(V, 1, 1, 1);"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("interp3(V, 2, 1, 1);"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("interp3(V, 1, 2, 2);"), 7.0, 1e-12);
    EXPECT_NEAR(evalScalar("interp3(V, 2, 2, 2);"), 8.0, 1e-12);
}

TEST_F(InterpTest, Interp3LinearKnownPlane)
{
    // f(x, y, z) = x + 2y + 3z → trilinear is exact on a regular grid.
    // Build V(r, c, p) = c + 2r + 3p (1-based to match implicit grid).
    eval("V = zeros(3, 3, 3);"
         "for r = 1:3, for c = 1:3, for p = 1:3,"
         "  V(r, c, p) = c + 2 * r + 3 * p; "
         "end, end, end");
    EXPECT_NEAR(evalScalar("interp3(V, 1.5, 1.0, 1.0);"),
                1.5 + 2.0 + 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("interp3(V, 2.7, 1.3, 2.4);"),
                2.7 + 2.0 * 1.3 + 3.0 * 2.4, 1e-10);
}

TEST_F(InterpTest, Interp3Nearest)
{
    eval("V = zeros(2, 2, 2);"
         "V(:,:,1) = [10 20; 30 40];"
         "V(:,:,2) = [50 60; 70 80];");
    // (1.7, 1.2, 1.4) → nearest (2, 1, 1) → V(1, 2, 1) = 20.
    EXPECT_DOUBLE_EQ(evalScalar("interp3(V, 1.7, 1.2, 1.4, 'nearest');"), 20.0);
}

TEST_F(InterpTest, Interp3OutOfGridIsNaN)
{
    eval("V = ones(2, 2, 2);"
         "y = interp3(V, 0.5, 1, 1);");
    EXPECT_TRUE(std::isnan(evalScalar("y;")));
}

TEST_F(InterpTest, InterpnDispatchesToInterp2For2D)
{
    eval("V = [10 20; 30 40];"
         "y = interpn(V, 1, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("y;"), 10.0);
}

TEST_F(InterpTest, InterpnDispatchesToInterp3For3D)
{
    eval("V = zeros(2, 2, 2);"
         "V(:,:,1) = [10 20; 30 40];"
         "V(:,:,2) = [50 60; 70 80];"
         "y = interpn(V, 2, 2, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y;"), 80.0);
}
