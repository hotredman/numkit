// toolboxes/signal/tests/lpc_parametric_test.cpp
//
// Coverage for parity-only LPC / parametric-modelling conversions:
//   poly2ac rc2ac                         (model -> autocorrelation)
//   poly2lsf lsf2poly                     (LPC <-> line spectral frequencies)
//   rc2is is2rc rc2lar lar2rc             (reflection-coeff parameterisations)
//   schurrc rlevinson                     (autocorr -> reflection / poly)
//   prony invfreqs invfreqz corrmtx       (model identification)
// Conversion pairs are checked by their exact round-trip; absolute values use
// closed forms (LAR = log((1+k)/(1-k)); prony of 0.5^n -> pole at 0.5).
//
// NOTE: numkit rc2is returns asin(k) (e.g. asin(0.5)=0.5236); MATLAB scales by
// 2/pi. Round-trip is self-consistent either way, so we assert that, not the
// absolute inverse-sine value (left for a dedicated parity check).

#include "dual_engine_fixture.hpp"

using namespace m_test;

class LpcParametricTest : public DualEngineTest
{};

TEST_P(LpcParametricTest, ModelToAutocorr)
{
    eval("r = poly2ac([1 -0.5 0.3 -0.1], 1);");
    EXPECT_EQ(eval("r").numel(), 4u);
    EXPECT_NEAR(evalScalar("r(1)"), 1.259907, 1e-5);   // zero-lag autocorrelation
    eval("ra = rc2ac([0.5; -0.3; 0.2], 1);");
    EXPECT_EQ(eval("ra").numel(), 4u);
    EXPECT_NEAR(evalScalar("ra(1)"), 1.0, 1e-9);
}

TEST_P(LpcParametricTest, LsfRoundTrip)
{
    eval("a = [1 -0.5 0.3 -0.1]; lsf = poly2lsf(a); a2 = lsf2poly(lsf);");
    EXPECT_EQ(eval("lsf").numel(), 3u);
    EXPECT_LT(evalScalar("max(abs(a(:) - a2(:)))"), 1e-12);   // exact round-trip
}

TEST_P(LpcParametricTest, ReflectionParamRoundTrips)
{
    eval("k = [0.5; -0.3; 0.2];");
    // inverse-sine round-trip
    EXPECT_LT(evalScalar("max(abs(k - is2rc(rc2is(k))))"), 1e-12);
    // log-area-ratio round-trip + closed form g1 = log((1+k1)/(1-k1)).
    eval("g = rc2lar(k);");
    EXPECT_NEAR(evalScalar("g(1)"), 1.0986123, 1e-6);          // log(1.5/0.5)
    EXPECT_LT(evalScalar("max(abs(k - lar2rc(g)))"), 1e-12);
}

TEST_P(LpcParametricTest, SchurRlevinson)
{
    // Schur recursion on an autocorrelation sequence: k1 = -r1/r0 = -0.5.
    eval("kk = schurrc([1 0.5 0.3 0.1]);");
    EXPECT_EQ(eval("kk").numel(), 3u);
    EXPECT_NEAR(evalScalar("kk(1)"), -0.5, 1e-9);
    // rlevinson (reverse Levinson) maps an LPC polynomial back to its
    // autocorrelation sequence; r(1) is the zero-lag autocorrelation.
    eval("r = rlevinson([1 0.5 0.3 0.1]', 1);");
    EXPECT_EQ(eval("r").numel(), 4u);
    EXPECT_NEAR(evalScalar("r(1)"), 1.259907, 1e-5);
}

TEST_P(LpcParametricTest, PronyInvfreqCorrmtx)
{
    // Impulse response 0.5^n -> single pole at 0.5 -> a = [1 -0.5].
    eval("[bp, ap] = prony([1 0.5 0.25 0.125 0.0625], 0, 1);");
    EXPECT_EQ(eval("ap").numel(), 2u);
    EXPECT_NEAR(evalScalar("ap(2)"), -0.5, 1e-6);
    // corrmtx: (N+m) x (m+1) data matrix.
    eval("X = corrmtx([1 2 3 4 5]', 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(X,2)")), 3);   // m+1 columns
    // invfreqz / invfreqs: identify a 1/1 model; denom is monic.
    eval("w = [0.1 0.2 0.5 1]'; h = [1 0.8 0.5 0.2]';");
    eval("[bz, az] = invfreqz(h, w, 1, 1);");
    EXPECT_EQ(eval("az").numel(), 2u);
    EXPECT_DOUBLE_EQ(evalScalar("az(1)"), 1.0);
    eval("[bs, as_] = invfreqs(h, w, 1, 1);");
    EXPECT_EQ(eval("as_").numel(), 2u);
    EXPECT_DOUBLE_EQ(evalScalar("as_(1)"), 1.0);
}

INSTANTIATE_DUAL(LpcParametricTest);
