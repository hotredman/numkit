// toolboxes/builtin/tests/residue_test.cpp
//
// Regression guard for residue (s-domain PFE) + residuez (z-domain PFE).
// v1 supports distinct poles only.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class ResidueTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── residue (s-domain) ─────────────────────────────────────────────

// (-4 s + 8) / (s² + 6 s + 8) = -4 s + 8 over (s+2)(s+4).
// Cover-up: at s = -2 → r = 16/2 = 8;  at s = -4 → r = 24/(-2) = -12.
TEST_F(ResidueTest, ResidueSimplePolesNoDirectTerm)
{
    eval("[r, p, k] = residue([-4 8], [1 6 8]);"
         "sr = sort(r); sp = sort(p);");
    EXPECT_NEAR(evalScalar("sr(1)"), -12.0, 1e-10);
    EXPECT_NEAR(evalScalar("sr(2)"),   8.0, 1e-10);
    EXPECT_NEAR(evalScalar("sp(1)"), -4.0,  1e-10);
    EXPECT_NEAR(evalScalar("sp(2)"), -2.0,  1e-10);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(k)")), 0);
}

// (2s³ + 5s² + 3s + 6) / (s+1)(s+2)(s+3) = 2 + residue terms.
TEST_F(ResidueTest, ResidueWithDirectTerm)
{
    eval("[r, p, k] = residue([2 5 3 6], [1 6 11 6]);"
         "sr = sort(r); sp = sort(p);");
    EXPECT_NEAR(evalScalar("sr(1)"), -6.0, 1e-9);
    EXPECT_NEAR(evalScalar("sr(2)"), -4.0, 1e-9);
    EXPECT_NEAR(evalScalar("sr(3)"),  3.0, 1e-9);
    EXPECT_NEAR(evalScalar("sp(1)"), -3.0, 1e-9);
    EXPECT_NEAR(evalScalar("sp(2)"), -2.0, 1e-9);
    EXPECT_NEAR(evalScalar("sp(3)"), -1.0, 1e-9);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(k)")), 1);
    EXPECT_NEAR(evalScalar("k(1)"), 2.0, 1e-12);
}

// Reconstruction identity (Heaviside): sum r_i / (s - p_i) at any
// non-pole s should equal B(s)/A(s) (when k is empty).
TEST_F(ResidueTest, ResidueHeavisideReconstructsBOverA)
{
    eval("b = [-4 8]; a = [1 6 8]; [r, p, k] = residue(b, a);"
         "s = 1; H_pfe = sum(r ./ (s - p));"
         "H_ref = polyval(b, s) / polyval(a, s);"
         "err = abs(H_pfe - H_ref);");
    EXPECT_LT(evalScalar("err"), 1e-10);
}

// Repeated poles are SUPPORTED since 2026-09-06 (the old stub threw
// here). Live coverage: ResidueDoublePole* / ResidueTriplePole below.

// ── residuez (z-domain) ────────────────────────────────────────────

// 1 / (1 - 0.5·z^-1)  →  r = 1, p = 0.5, k = [].
TEST_F(ResidueTest, ResiduezSinglePoleIIR)
{
    eval("[r, p, k] = residuez([1], [1 -0.5]);");
    EXPECT_NEAR(evalScalar("r(1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("p(1)"), 0.5, 1e-12);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(k)")), 0);
}

// (1 + 0.5·z^-1) / (1 - 0.25·z^-2). Poles at z = ±0.5.
// Cover-up: r_i = B(p_i) · p_i / (p_i - p_{j}).
//   B(0.5)  = 1 + 0.5/0.5  = 2;   denom = 0.5 - (-0.5) = 1;  r_1 = 2 · 0.5 / 1 =  1.0
//   B(-0.5) = 1 + 0.5/(-0.5) = 0; denom = -1; r_2 = 0
TEST_F(ResidueTest, ResiduezTwoPolesIIR)
{
    eval("[r, p, k] = residuez([1 0.5], [1 0 -0.25]);"
         "sp = sort(p); sr = r;"  // we'll test by reconstruction below
         "z = 2; "                  // arbitrary non-pole test point
         "H_pfe = sum(r ./ (1 - p .* z^-1));"
         "H_ref = (1 + 0.5 * z^-1) / (1 - 0.25 * z^-2);"
         "err = abs(H_pfe - H_ref);");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

// (Superseded 2026-09-06: residuez now SUPPORTS improper TFs (direct
// terms via deconv) and repeated poles — live coverage:
// ResiduezRepeatedPoles + ResiduezImproperDirectTerm.)

// ── residue repeated poles (MATLAB R2025b probed 2026-09-06) ──────────
// Within a repeated group MATLAB returns coefficients in ASCENDING
// power order: r = [c1 ... cm] for c1/(s-p) + ... + cm/(s-p)^m.
TEST_F(ResidueTest, ResidueDoublePoleUnit)
{
    eval("[r, p, k] = residue([1], [1 2 1]);");      // 1/(s+1)^2
    EXPECT_NEAR(evalScalar("r(1)"), 0.0, 1e-10);
    EXPECT_NEAR(evalScalar("r(2)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("p(1)"), -1.0, 1e-10);
    EXPECT_NEAR(evalScalar("p(2)"), -1.0, 1e-10);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(k)")), 0);
}

TEST_F(ResidueTest, ResidueDoublePoleWithNumerator)
{
    eval("[r, p] = residue([1 0], [1 2 1]);");       // s/(s+1)^2
    EXPECT_NEAR(evalScalar("r(1)"),  1.0, 1e-10);
    EXPECT_NEAR(evalScalar("r(2)"), -1.0, 1e-10);
    EXPECT_NEAR(evalScalar("p(1)"), -1.0, 1e-10);
}

TEST_F(ResidueTest, ResidueTriplePole)
{
    eval("[r, p] = residue([1 0 0], [1 3 3 1]);");   // s^2/(s+1)^3
    EXPECT_NEAR(evalScalar("r(1)"),  1.0, 1e-9);
    EXPECT_NEAR(evalScalar("r(2)"), -2.0, 1e-9);
    EXPECT_NEAR(evalScalar("r(3)"),  1.0, 1e-9);
    EXPECT_NEAR(evalScalar("p(3)"), -1.0, 1e-10);
    eval("[r4, ~] = residue([1 1 1], [1 3 3 1]);");  // (s^2+s+1)/(s+1)^3
    EXPECT_NEAR(evalScalar("r4(1)"),  1.0, 1e-9);
    EXPECT_NEAR(evalScalar("r4(2)"), -1.0, 1e-9);
    EXPECT_NEAR(evalScalar("r4(3)"),  1.0, 1e-9);
}

// bugs/closed/signal/residuez-repeated-poles.md (FIXED; live guard) —
// the impulse/S-matrix port of residuez.m. Values MATLAB-probed
// 2026-09-06; MATLAB's own repeated-pole numbers carry root-finder dust
// (its triple-pole probe prints poles spread 7e-6), so tolerances are
// dust-scale while the structural values are analytic.
TEST_F(ResidueTest, ResiduezRepeatedPoles)
{
    // 1/(1-z^-1)^2 -> r = [0, 1] ascending power order.
    eval("[r, p, k] = residuez([1], [1 -2 1]);");
    EXPECT_NEAR(evalScalar("r(1)"), 0.0, 1e-6);
    EXPECT_NEAR(evalScalar("r(2)"), 1.0, 1e-9);
    EXPECT_NEAR(evalScalar("p(1)"), 1.0, 1e-6);
    EXPECT_NEAR(evalScalar("p(2)"), 1.0, 1e-6);
    // (1+2z^-1)/(1-z^-1)^3 -> r = [0, -2, 3].
    eval("[r3, p3] = residuez([1 2], [1 -3 3 -1]);");
    EXPECT_NEAR(evalScalar("r3(1)"),  0.0, 1e-3);
    EXPECT_NEAR(evalScalar("r3(2)"), -2.0, 1e-3);
    EXPECT_NEAR(evalScalar("r3(3)"),  3.0, 1e-3);
    // Mixed: double at 1 + simples at 2 and -1 (poles DESC |p|).
    eval("[r4, p4] = residuez([2 5 3 6], [1 -3 1 3 -2]);");
    EXPECT_NEAR(evalScalar("p4(1)"), 2.0, 1e-9);
    EXPECT_NEAR(evalScalar("p4(4)"), -1.0, 1e-9);
    EXPECT_NEAR(evalScalar("r4(1)"), 16.0, 1e-9);
    EXPECT_NEAR(evalScalar("r4(2)"), -5.5, 1e-4);
    EXPECT_NEAR(evalScalar("r4(3)"), -8.0, 1e-6);
    EXPECT_NEAR(evalScalar("r4(4)"), -0.5, 1e-9);
}

// residuez improper TF: direct terms via deconv on flipped polynomials
// (MATLAB supports numel(b) > numel(a); the old v1 threw). Probed: a
// direct feedthrough of 2 plus the proper part's expansion.
TEST_F(ResidueTest, ResiduezImproperDirectTerm)
{
    eval("[r, p, k] = residuez([2 1 0], [1 -0.5]);");
    // MATLAB-probed: -2 + 0*z^-1 + 4/(1-0.5 z^-1).
    EXPECT_NEAR(evalScalar("numel(k)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("k(1)"), -2.0, 1e-9);
    EXPECT_NEAR(evalScalar("k(2)"), 0.0, 1e-9);
    EXPECT_NEAR(evalScalar("r(1)"), 4.0, 1e-9);
    EXPECT_NEAR(evalScalar("p(1)"), 0.5, 1e-9);
}
