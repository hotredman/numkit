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
    void SetUp() override { engine.eval("import compat.*;"); }
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

// Repeated pole (s-1)² → throws.
TEST_F(ResidueTest, ResidueRepeatedPoleThrows)
{
    EXPECT_THROW(eval("residue([1], [1 -2 1]);"), std::exception);
}

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
    // Pin the pole locations.
    EXPECT_NEAR(evalScalar("sp(1)"), -0.5, 1e-12);
    EXPECT_NEAR(evalScalar("sp(2)"),  0.5, 1e-12);
}

TEST_F(ResidueTest, ResiduezImproperTFThrows)
{
    // numel(b) > numel(a) — direct term in z^-1 is a v1 gap.
    EXPECT_THROW(eval("residuez([1 2 3], [1 -0.5]);"), std::exception);
}

TEST_F(ResidueTest, ResiduezRepeatedPoleThrows)
{
    // (1 - 0.5·z^-1)² — repeated pole at z = 0.5.
    EXPECT_THROW(eval("residuez([1], [1 -1 0.25]);"), std::exception);
}
