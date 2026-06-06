// libs/ode/tests/ode45_test.cpp
//
// Regression guard for ode45 — Dormand-Prince 5(4) with Shampine's
// 4th-order free dense-output interpolant. Pinned against MATLAB
// R2025b (tools/parity/specs/ode45.json) and analytical solutions.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class Ode45Test : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// y' = -y, y(0) = 1, tspan = [0 2]. Analytical y(2) = exp(-2).
// At default tols the achieved accuracy must be ≤ RelTol·|y|.
TEST_F(Ode45Test, ScalarExpDecayDefaultTol)
{
    eval("[t, y] = ode45(@(t,y) -y, [0 2], 1);");
    EXPECT_NEAR(evalScalar("t(end)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(-2.0), 1e-3); // RelTol = 1e-3
    // Refine = 4 default: at least ~4 sample points per integration step.
    EXPECT_GE(static_cast<int>(evalScalar("length(t)")), 8);
    // shape: (n × 1).
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y,2)")), 1);
}

// 2-D harmonic oscillator: y1' = y2, y2' = -y1; y(0) = [1; 0]; t in [0 pi].
// Analytical y(pi) = [cos(pi); -sin(pi)] = [-1; 0].
TEST_F(Ode45Test, HarmonicOscillator2D)
{
    eval("[t, y] = ode45(@(t,y) [y(2); -y(1)], [0 pi], [1; 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y,2)")), 2);
    EXPECT_NEAR(evalScalar("y(end,1)"), -1.0, 1e-3);
    EXPECT_NEAR(evalScalar("y(end,2)"),  0.0, 1e-3);
}

// Explicit tspan: n requested sample points → n output rows; values
// interpolated via Shampine's 4th-order dense output.
TEST_F(Ode45Test, ExplicitTspanReturnsRequestedRows)
{
    eval("opts = odeset('RelTol', 1e-9, 'AbsTol', 1e-12);"
         "[t, y] = ode45(@(t,y) -y, linspace(0,1,6), 1, opts);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(t)")), 6);
    EXPECT_NEAR(evalScalar("t(1)"),   0.0, 1e-14);
    EXPECT_NEAR(evalScalar("t(end)"), 1.0, 1e-14);
    EXPECT_NEAR(evalScalar("y(1)"),   1.0,            1e-12);
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(-1.0), 1e-7);
    // Mid-points via dense output.
    EXPECT_NEAR(evalScalar("y(3)"), std::exp(-0.4), 1e-7);
}

// Tight tolerance → accuracy at the requested level.
TEST_F(Ode45Test, TightToleranceAccuracy)
{
    eval("opts = odeset('RelTol', 1e-10, 'AbsTol', 1e-12);"
         "[t, y] = ode45(@(t,y) -y, [0 5], 1, opts);");
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(-5.0), 1e-9);
}

// Reverse integration: tspan[end] < tspan[1].
TEST_F(Ode45Test, ReverseIntegration)
{
    eval("[t, y] = ode45(@(t,y) -y, [0 -1], 1);");
    EXPECT_NEAR(evalScalar("t(end)"), -1.0, 1e-12);
    // y(-1) = exp(1) ≈ 2.71828
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(1.0), 1e-3);
}

// MaxStep cap.
TEST_F(Ode45Test, MaxStepCapsStepSize)
{
    eval("opts = odeset('MaxStep', 0.1);"
         "[t, y] = ode45(@(t,y) -y, [0 2], 1, opts);");
    // 20 actual integration steps of size 0.1, ×4 refine → 81 output rows.
    EXPECT_EQ(static_cast<int>(evalScalar("length(t)")), 81);
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(-2.0), 1e-3);
}

// Refine = 1: emit only accepted-step endpoints (no interpolation).
TEST_F(Ode45Test, RefineOneDisablesInterpolation)
{
    eval("opts = odeset('Refine', 1);"
         "[t, y] = ode45(@(t,y) -y, [0 2], 1, opts);");
    // With Refine = 1, length(t) ≈ number of accepted steps + 1.
    const int n = static_cast<int>(evalScalar("length(t)"));
    EXPECT_GE(n, 3);
    EXPECT_LE(n, 20);
    EXPECT_NEAR(evalScalar("t(end)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(-2.0), 1e-3);
}

// AbsTol vector (one per component).
TEST_F(Ode45Test, AbsTolVectorPerComponent)
{
    eval("opts = odeset('RelTol', 1e-8, 'AbsTol', [1e-10, 1e-12]);"
         "[t, y] = ode45(@(t,y) [y(2); -y(1)], [0 pi/2], [1; 0], opts);");
    // y(pi/2) = [cos, -sin] = [0, -1]
    EXPECT_NEAR(evalScalar("y(end,1)"),  0.0, 1e-6);
    EXPECT_NEAR(evalScalar("y(end,2)"), -1.0, 1e-6);
}

// Stiff-spring-style nonlinear (Van der Pol with mu=0.5, mildly stiff).
TEST_F(Ode45Test, VanDerPolMildlyStiff)
{
    eval("opts = odeset('RelTol', 1e-8, 'AbsTol', 1e-10);"
         "mu = 0.5;"
         "vdp = @(t,y) [y(2); mu*(1 - y(1)^2)*y(2) - y(1)];"
         "[t, y] = ode45(vdp, [0 5], [2; 0], opts);");
    // No closed form, but conservation of phase-space measure means
    // |y| stays bounded. Endpoint is reproducible to ~1e-6 across runs.
    EXPECT_GE(static_cast<int>(evalScalar("size(y,1)")), 10);
    EXPECT_LT(std::fabs(evalScalar("y(end,1)")), 5.0);
}

// nargout = 1 (only t).
TEST_F(Ode45Test, NargoutOneReturnsTimesOnly)
{
    eval("t = ode45(@(t,y) -y, [0 2], 1);");
    EXPECT_NEAR(evalScalar("t(end)"), 2.0, 1e-12);
}
