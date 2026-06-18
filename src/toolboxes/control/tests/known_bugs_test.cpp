// toolboxes/control/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/control/*.md. Disabled until
// fixed; remove `DISABLED_` to turn into a live regression guard.
// MATLAB R2025b reference values.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ControlKnownBug : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// bugs/control/lqr-hinfnorm.md — LQR gain via the CARE.
// FIXED 2026-06-19 (wraps care) — promoted live.
TEST_F(ControlKnownBug, Lqr)
{
    eval("K = lqr([0 1; 0 0], [0; 1], eye(2), 1);");
    EXPECT_NEAR(evalScalar("K(1)"), 1.000000, 1e-5);
    EXPECT_NEAR(evalScalar("K(2)"), 1.732051, 1e-5);
}

// bugs/control/hinfnorm.md — H-infinity norm (Inf for poles on jω axis).
// FIXED 2026-06-19 (Hamiltonian bisection) — promoted live.
TEST_F(ControlKnownBug, Hinfnorm)
{
    eval("g = hinfnorm(ss([0 1; -1 0], [0; 1], [1 0], 0));");
    EXPECT_TRUE(std::isinf(evalScalar("g")));
}

// bugs/control/lqr-hinfnorm.md — dlqr (discrete LQR) missing.
// FIXED 2026-06-19 (wraps dare) — promoted live.
TEST_F(ControlKnownBug, Dlqr)
{
    eval("K = dlqr([0.9 0.1; 0 0.8], [0; 1], eye(2), 1);");
    EXPECT_NEAR(evalScalar("sum(K)"), 0.71004388, 1e-5);
}

// bugs/control/lqr-hinfnorm.md — gram (controllability/observability gramian).
// FIXED 2026-06-19 (wraps lyap/dlyap) — promoted live.
TEST_F(ControlKnownBug, Gram)
{
    eval("W = gram(ss([-1 0; 0 -2], [1; 1], [1 1], 0), 'c');");
    EXPECT_NEAR(evalScalar("sum(W(:))"), 1.4166667, 1e-5);
}

// bugs/control/care-dare.md — continuous/discrete algebraic Riccati solvers.
// FIXED 2026-06-18 (matrix sign-function method) — promoted live.
TEST_F(ControlKnownBug, CareDare)
{
    eval("X = care([0 1; 0 0], [0; 1], eye(2));");
    EXPECT_NEAR(evalScalar("trace(X)"), 3.46410161513776, 1e-8);
    EXPECT_NEAR(evalScalar("X(1,1)"),   1.73205080756888, 1e-8);  // sqrt(3)
    eval("Y = dare([1 1; 0 1], [0; 1], eye(2), 1);");
    EXPECT_NEAR(evalScalar("trace(Y)"), 7.56025722770319, 1e-7);
}

// bugs/control/minreal.md — minimal realization (pole/zero cancellation).
// FIXED 2026-06-19 (root cancellation) — promoted live.
TEST_F(ControlKnownBug, Minreal)
{
    // (s+1)/(s+1)^2 -> 1/(s+1)
    eval("sysr = minreal(tf([1 1], [1 2 1])); [n, d] = tfdata(sysr, 'v');");
    EXPECT_NEAR(evalScalar("n(end)"), 1.0, 1e-9);
    EXPECT_NEAR(evalScalar("d(1)"),   1.0, 1e-9);
    EXPECT_NEAR(evalScalar("d(end)"), 1.0, 1e-9);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d)")), 2);   // reduced to first order
}

// bugs/control/initial.md — initial-condition response (zero input).
// FIXED 2026-06-19 (zero-input simulate + x0) — promoted live.
TEST_F(ControlKnownBug, Initial)
{
    eval("[y, t] = initial(ss(-2, 0, 1, 0), 1);");   // y = e^{-2t}
    EXPECT_NEAR(evalScalar("y(1)"),   1.0, 1e-9);
    EXPECT_NEAR(evalScalar("y(end)"), 0.00301995172040398, 1e-6);
}

// bugs/control/allmargin.md — all stability margins as a struct.
// FIXED 2026-06-19 (exact G(jω) scan + bisection) — promoted live.
TEST_F(ControlKnownBug, Allmargin)
{
    eval("S = allmargin(tf(1, [1 6 11 6]));");   // 1/((s+1)(s+2)(s+3))
    EXPECT_DOUBLE_EQ(evalScalar("double(S.Stable)"), 1.0);
    EXPECT_NEAR(evalScalar("S.GainMargin(1)"),  60.0,            1e-2);
    EXPECT_NEAR(evalScalar("S.GMFrequency(1)"), 3.31662561934,   1e-6);  // sqrt(11)
}

// bugs/control/zpk-empty-zeros.md — zpk with no finite zeros drops gain k.
// FIXED 2026-06-19 (zp2tf empty-zero → num=[k]) — promoted live.
TEST_F(ControlKnownBug, ZpkEmptyZerosGain)
{
    eval("[n, d] = tfdata(zpk([], [-1 -2], 2), 'v');");
    EXPECT_NEAR(evalScalar("n(end)"), 2.0, 1e-12);   // numkit: 0 (gain dropped)
    EXPECT_NEAR(evalScalar("d(1)"),   1.0, 1e-12);
    EXPECT_NEAR(evalScalar("d(end)"), 2.0, 1e-12);
}

// bugs/control/covar.md — output covariance from white-noise input.
TEST_F(ControlKnownBug, DISABLED_Covar)
{
    EXPECT_NEAR(evalScalar("covar(ss(-1, 1, 1, 0), 1)"), 0.5, 1e-9);
}
