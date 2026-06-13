// toolboxes/control/tests/control_core_test.cpp
//
// Coverage for the control core algebra that previously had no gtest
// (parity-spec only): Lyapunov solvers (lyap/dlyap), state-space structure
// (ctrb/obsv), pole placement (place/acker), and system properties
// (pole/zero/dcgain/damp). Exact values come from the parity specs / closed
// form; where the closed form is awkward (matrix Lyapunov, pole placement) we
// assert the defining residual / closed-loop spectrum instead, which is
// convention-independent.

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class ControlCoreTest : public DualEngineTest
{};

// ── Lyapunov: lyap (AX + XA' + Q = 0), dlyap (AXA' - X + Q = 0) ──

TEST_P(ControlCoreTest, LyapScalar)
{
    // -X - X + 1 = 0 → X = 0.5  (parity: lyap(-1,1))
    EXPECT_NEAR(evalScalar("lyap(-1, 1)"), 0.5, 1e-12);
}

TEST_P(ControlCoreTest, DlyapScalar)
{
    // 0.25X - X + 1 = 0 → X = 4/3  (parity: dlyap(0.5,1))
    EXPECT_NEAR(evalScalar("dlyap(0.5, 1)"), 4.0 / 3.0, 1e-12);
}

TEST_P(ControlCoreTest, LyapMatrixResidual)
{
    eval("A = [-2 1; 0 -3]; Q = eye(2); X = lyap(A, Q); r = norm(A*X + X*A' + Q);");
    EXPECT_NEAR(evalScalar("r"), 0.0, 1e-9);
    // Lyapunov solution is symmetric for symmetric Q.
    EXPECT_NEAR(evalScalar("norm(X - X')"), 0.0, 1e-9);
}

TEST_P(ControlCoreTest, DlyapMatrixResidual)
{
    eval("A = [0.5 0.1; 0 0.3]; Q = eye(2); X = dlyap(A, Q); r = norm(A*X*A' - X + Q);");
    EXPECT_NEAR(evalScalar("r"), 0.0, 1e-9);
}

// ── State-space structure: ctrb [B AB ...], obsv [C; CA; ...] ──

TEST_P(ControlCoreTest, CtrbSiso)
{
    // [B  A*B] with A=[1 2;3 4], B=[5;6] → AB=[17;39]
    eval("C = ctrb([1 2; 3 4], [5; 6]);");
    EXPECT_EQ(eval("C").dims().rows(), 2u);
    EXPECT_EQ(eval("C").dims().cols(), 2u);
    EXPECT_NEAR(evalScalar("C(1,1)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("C(2,1)"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("C(1,2)"), 17.0, 1e-12);
    EXPECT_NEAR(evalScalar("C(2,2)"), 39.0, 1e-12);
}

TEST_P(ControlCoreTest, ObsvSiso)
{
    // [C; C*A] with A=[1 2;3 4], C=[5 6] → CA=[23 34]
    eval("O = obsv([1 2; 3 4], [5 6]);");
    EXPECT_NEAR(evalScalar("O(1,1)"), 5.0, 1e-12);
    EXPECT_NEAR(evalScalar("O(1,2)"), 6.0, 1e-12);
    EXPECT_NEAR(evalScalar("O(2,1)"), 23.0, 1e-12);
    EXPECT_NEAR(evalScalar("O(2,2)"), 34.0, 1e-12);
}

// ── Pole placement: verify the closed-loop spectrum eig(A - B*K) ──

TEST_P(ControlCoreTest, PlacePolesDoubleIntegrator)
{
    eval("A = [0 1; 0 0]; B = [0; 1]; K = place(A, B, [-1 -2]); e = sort(real(eig(A - B*K)));");
    EXPECT_NEAR(evalScalar("e(1)"), -2.0, 1e-6);
    EXPECT_NEAR(evalScalar("e(2)"), -1.0, 1e-6);
}

TEST_P(ControlCoreTest, AckerPolesDoubleIntegrator)
{
    eval("A = [0 1; 0 0]; B = [0; 1]; K = acker(A, B, [-1 -2]); e = sort(real(eig(A - B*K)));");
    EXPECT_NEAR(evalScalar("e(1)"), -2.0, 1e-6);
    EXPECT_NEAR(evalScalar("e(2)"), -1.0, 1e-6);
}

// ── System properties: pole / zero / dcgain / damp ──

TEST_P(ControlCoreTest, PoleFirstOrder)
{
    // tf(1,[1 1]) → pole at s = -1  (parity)
    eval("p = pole(tf(1, [1 1]));");
    EXPECT_NEAR(evalScalar("real(p(1))"), -1.0, 1e-9);
}

TEST_P(ControlCoreTest, ZeroOfNumerator)
{
    // tf([1 2],[1 3 2]) → numerator root s = -2
    eval("z = zero(tf([1 2], [1 3 2]));");
    EXPECT_NEAR(evalScalar("real(z(1))"), -2.0, 1e-9);
}

TEST_P(ControlCoreTest, DcGainUnity)
{
    // H(0) = 1/1 = 1
    EXPECT_NEAR(evalScalar("dcgain(tf(1, [1 2 1]))"), 1.0, 1e-9);
}

TEST_P(ControlCoreTest, DampCriticallyDamped)
{
    // (s+1)^2 → two poles at -1: wn = 1, zeta = 1
    eval("[wn, zeta] = damp(tf(1, [1 2 1]));");
    EXPECT_EQ(eval("wn").numel(), 2u);
    EXPECT_NEAR(evalScalar("wn(1)"), 1.0, 1e-6);
    EXPECT_NEAR(evalScalar("zeta(1)"), 1.0, 1e-6);
}

INSTANTIATE_DUAL(ControlCoreTest);
