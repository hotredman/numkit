// libs/ode/tests/ode23_test.cpp
//
// Regression guard for ode23 — Bogacki-Shampine 3(2) with cubic
// Hermite dense-output interpolant. Pinned against MATLAB R2025b
// (tools/parity/specs/ode23.json) and analytical solutions.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class Ode23Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// y' = -y, y(0) = 1, tspan = [0 2]. Default tols: y(2) ≈ exp(-2)
// within ~RelTol/error contract (default Refine = 1 for ode23,
// unlike ode45's 4).
TEST_F(Ode23Test, ScalarExpDecayDefaultTol)
{
    eval("[t, y] = ode23(@(t,y) -y, [0 2], 1);");
    EXPECT_NEAR(evalScalar("t(end)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(-2.0), 5e-3);
    EXPECT_GE(static_cast<int>(evalScalar("length(t)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(t,2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y,2)")), 1);
}

TEST_F(Ode23Test, HarmonicOscillator2D)
{
    eval("[t, y] = ode23(@(t,y) [y(2); -y(1)], [0 pi], [1; 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y,2)")), 2);
    EXPECT_NEAR(evalScalar("y(end,1)"), -1.0, 5e-3);
    EXPECT_NEAR(evalScalar("y(end,2)"),  0.0, 5e-3);
}

TEST_F(Ode23Test, ExplicitTspanReturnsRequestedRows)
{
    eval("opts = odeset('RelTol', 1e-9, 'AbsTol', 1e-12);"
         "[t, y] = ode23(@(t,y) -y, linspace(0,1,6), 1, opts);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(t)")), 6);
    EXPECT_NEAR(evalScalar("t(end)"), 1.0, 1e-14);
    EXPECT_NEAR(evalScalar("y(1)"),   1.0,            1e-12);
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(-1.0), 1e-7);
    EXPECT_NEAR(evalScalar("y(3)"),   std::exp(-0.4), 1e-7);
}

TEST_F(Ode23Test, TightToleranceAccuracy)
{
    eval("opts = odeset('RelTol', 1e-10, 'AbsTol', 1e-12);"
         "[t, y] = ode23(@(t,y) -y, [0 5], 1, opts);");
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(-5.0), 1e-8);
}

TEST_F(Ode23Test, ReverseIntegration)
{
    eval("[t, y] = ode23(@(t,y) -y, [0 -1], 1);");
    EXPECT_NEAR(evalScalar("t(end)"), -1.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(1.0), 5e-3);
}

TEST_F(Ode23Test, MaxStepCapsStepSize)
{
    eval("opts = odeset('MaxStep', 0.1);"
         "[t, y] = ode23(@(t,y) -y, [0 2], 1, opts);");
    // 20 actual integration steps of size 0.1, default Refine = 1
    // for ode23 → output rows ≈ 22 (matches MATLAB).
    EXPECT_GE(static_cast<int>(evalScalar("length(t)")), 20);
    EXPECT_LE(static_cast<int>(evalScalar("length(t)")), 25);
    EXPECT_NEAR(evalScalar("y(end)"), std::exp(-2.0), 5e-3);
}

// ode23 default Refine = 1, so MaxStep override of Refine=4 should
// quadruple output.
TEST_F(Ode23Test, RefineFourExpandsOutput)
{
    eval("opts1 = odeset('Refine', 1);"
         "[t1, y1] = ode23(@(t,y) -y, [0 2], 1, opts1);"
         "opts4 = odeset('Refine', 4);"
         "[t4, y4] = ode23(@(t,y) -y, [0 2], 1, opts4);");
    const int n1 = static_cast<int>(evalScalar("length(t1)"));
    const int n4 = static_cast<int>(evalScalar("length(t4)"));
    EXPECT_GT(n4, n1 * 3);    // ~4× more (n1*4 - 3 ideally)
    EXPECT_LT(n4, n1 * 5);
}

// AbsTol vector (one per component).
TEST_F(Ode23Test, AbsTolVectorPerComponent)
{
    eval("opts = odeset('RelTol', 1e-8, 'AbsTol', [1e-10, 1e-12]);"
         "[t, y] = ode23(@(t,y) [y(2); -y(1)], [0 pi/2], [1; 0], opts);");
    EXPECT_NEAR(evalScalar("y(end,1)"),  0.0, 1e-5);
    EXPECT_NEAR(evalScalar("y(end,2)"), -1.0, 1e-5);
}

// nargout = 1: only t returned.
TEST_F(Ode23Test, NargoutOneReturnsTimesOnly)
{
    eval("t = ode23(@(t,y) -y, [0 2], 1);");
    EXPECT_NEAR(evalScalar("t(end)"), 2.0, 1e-12);
}

// Validation errors.
TEST_F(Ode23Test, InvalidTspanThrows)
{
    EXPECT_THROW(eval("ode23(@(t,y) -y, [0], 1);"), std::exception);
    EXPECT_THROW(eval("ode23(@(t,y) -y, [1 1], 1);"), std::exception);
    EXPECT_THROW(eval("ode23(@(t,y) -y, [0 1 0.5 2], 1);"), std::exception);
}
