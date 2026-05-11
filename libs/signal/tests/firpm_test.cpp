// libs/signal/tests/firpm_test.cpp
//
// gtest unit coverage for firpm (Parks-McClellan optimal FIR design).
// Mirrors the parity spec fingerprints to guard against regressions
// in the Remez exchange iteration (initial-extremals seeding, peak
// detection at band edges, multiple-exchange alternation merge,
// barycentric Lagrange interpolation, inverse-DCT-I reconstruction).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class FirpmTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void   SetUp() override { engine.eval("import compat.*;"); }
    double eval_scalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// Order-20 Type-I lowpass — minimal smoke. Center coefficient and the
// first / last (mirror) entries pin both the polynomial scale and the
// symmetric reconstruction. err pins the ripple magnitude reported by
// Remez (|δ|).
TEST_F(FirpmTest, LowpassN20)
{
    engine.eval("[b, err] = firpm(20, [0 0.4 0.5 1], [1 1 0 0]);");
    EXPECT_NEAR(eval_scalar("b(1)"),   0.0387762, 5e-4);
    EXPECT_NEAR(eval_scalar("b(11)"),  0.448730,  5e-4);
    EXPECT_NEAR(eval_scalar("b(end)"), 0.0387762, 5e-4); // symmetry
    EXPECT_NEAR(eval_scalar("err"),    0.0548753, 5e-4);
    EXPECT_DOUBLE_EQ(eval_scalar("length(b)"), 21.0);
}

// Bandpass n=30, three bands stopband/passband/stopband — exercises
// 6 band edges plus interior peaks, classic equiripple shape.
TEST_F(FirpmTest, BandpassN30)
{
    engine.eval("b = firpm(30, [0 0.2 0.3 0.6 0.7 1], [0 0 1 1 0 0]);");
    EXPECT_NEAR(eval_scalar("b(1)"),  0.000955, 5e-4);
    EXPECT_NEAR(eval_scalar("b(16)"), 0.399038, 5e-4);
}

// Highpass n=30 with asymmetric band widths (stopband 0.3 wide,
// passband 0.6 wide). The asymmetry stresses the band-aware initial
// extremal distribution.
TEST_F(FirpmTest, HighpassN30)
{
    engine.eval("b = firpm(30, [0 0.3 0.4 1], [0 0 1 1]);");
    EXPECT_NEAR(eval_scalar("b(1)"),  0.010147, 5e-4);
    EXPECT_NEAR(eval_scalar("b(16)"), 0.649809, 5e-4);
}

// Weighted bandpass — passband W=1, stopbands W=10. Previously this
// case failed (algorithm converged to a sub-optimal local stationary
// at δ=0.027 instead of the MATLAB minimax δ=0.088) until band-edge
// indices were promoted to always-candidates in the peak finder.
// Pinning this case guards that regression specifically.
TEST_F(FirpmTest, WeightedBandpassN30)
{
    engine.eval("[b, err] = firpm(30, [0 0.2 0.3 0.6 0.7 1], "
                "[0 0 1 1 0 0], [10 1 10]);");
    EXPECT_NEAR(eval_scalar("b(1)"),  -0.008565, 1e-3);
    EXPECT_NEAR(eval_scalar("b(16)"),  0.379123, 1e-3);
    EXPECT_NEAR(eval_scalar("err"),    0.088270, 1e-3);
}

// Four-band weighted multi-band pass-stop-pass-stop with strong
// stopband weighting [1 5 1 10]. Exercises proportional band-width
// initial seed (one band has 6× the width-weight of another).
TEST_F(FirpmTest, MultibandWeightedN40)
{
    engine.eval("b = firpm(40, [0 0.18 0.22 0.42 0.45 0.65 0.68 1], "
                "[1 1 0 0 1 1 0 0], [1 5 1 10]);");
    EXPECT_NEAR(eval_scalar("b(1)"),  0.024548, 5e-4);
    EXPECT_NEAR(eval_scalar("b(21)"), 0.421307, 5e-4);
}

// Order N=0/1/2 — MATLAB throws "Filter order must be 3 or more";
// numkit must match the same lower bound.
TEST_F(FirpmTest, OrderTooSmallThrows)
{
    EXPECT_THROW(engine.eval("firpm(2, [0 1], [1 1]);"), std::exception);
}

// Odd order (Type II) — H(ω) = cos(ω/2) · Σ a[k] cos(kω). Reuses the
// Remez kernel after a Q-transformation D' = D / cos(ω/2), W' = W ·
// cos(ω/2). Reconstruction maps a[k] to half-integer-shifted cosine
// coefficients b[n] = (a[n] + a[n+1])/2 (special-cased at endpoints).
// Pinned against MATLAB R2025b probe values.
TEST_F(FirpmTest, OddOrderTypeIIMatchesMatlab)
{
    engine.eval("[b, err] = firpm(21, [0 0.4 0.5 1], [1 1 0 0]);");
    EXPECT_DOUBLE_EQ(eval_scalar("length(b)"), 22.0);
    EXPECT_LT(eval_scalar("max(abs(b - b(end:-1:1)))"), 1e-15);   // sym
    EXPECT_NEAR(eval_scalar("b(1)"),   0.0168747, 5e-4);
    EXPECT_NEAR(eval_scalar("b(11)"),  0.412952,  5e-4);
    EXPECT_NEAR(eval_scalar("b(end)"), 0.0168747, 5e-4);          // mirror
    EXPECT_NEAR(eval_scalar("err"),    0.055977,  5e-4);
}

// 'hilbert' ftype Type III (even N anti-symmetric) — Q(ω) = sin(ω).
// A(ω) = sin(ω)·Σ a[k] cos(kω); c[1] = a[0] - a[2]/2,
// c[n] = (a[n-1] - a[n+1])/2 for 2 ≤ n ≤ L+1. h[Lh - n] = -c[n]/2
// (MATLAB sign convention: low-index h negative when ideal A > 0).
TEST_F(FirpmTest, HilbertTypeIIIMatchesMatlab)
{
    engine.eval("[b, err] = firpm(20, [0.1 0.9], [1 1], 'hilbert');");
    EXPECT_DOUBLE_EQ(eval_scalar("length(b)"), 21.0);
    EXPECT_LT(eval_scalar("max(abs(b + b(end:-1:1)))"), 1e-12); // anti-sym
    EXPECT_NEAR(eval_scalar("b(6)"),  -0.0931764, 1e-3);
    EXPECT_NEAR(eval_scalar("b(11)"),  0.0,       1e-12);    // center=0
    EXPECT_NEAR(eval_scalar("b(18)"),  0.0478828, 1e-3);
    EXPECT_NEAR(eval_scalar("err"),    0.0227453, 5e-4);
}

// 'hilbert' Type IV (odd N anti-symmetric) — Q(ω) = sin(ω/2).
// h[k] = -h[N-k] (no zero center).
TEST_F(FirpmTest, HilbertTypeIVMatchesMatlab)
{
    engine.eval("[b, err] = firpm(21, [0.1 0.9], [1 1], 'hilbert');");
    EXPECT_DOUBLE_EQ(eval_scalar("length(b)"), 22.0);
    EXPECT_LT(eval_scalar("max(abs(b + b(end:-1:1)))"), 1e-12); // anti-sym
    EXPECT_NEAR(eval_scalar("b(1)"),   -0.0142424, 1e-3);
    EXPECT_NEAR(eval_scalar("b(11)"),  -0.6469107, 1e-3);
    EXPECT_NEAR(eval_scalar("b(22)"),   0.0142424, 1e-3);    // mirror
    EXPECT_NEAR(eval_scalar("err"),     0.0136600, 5e-4);
}

// Differentiator ftype — deferred (Remez δ ≠ MATLAB δ on probed
// cases; need MATLAB-specific D / W convention). Currently produces
// a valid anti-symmetric FIR but values diverge.
TEST_F(FirpmTest, DifferentiatorPartialNoThrow)
{
    // Doesn't throw — produces *some* anti-symmetric output.
    EXPECT_NO_THROW(engine.eval(
        "b = firpm(20, [0 0.9], [0 0.9], 'differentiator');"));
    EXPECT_DOUBLE_EQ(eval_scalar("length(b)"), 21.0);
}

// Symmetric impulse response — Type-I FIR (h[k] = h[N-k]). Robust
// guard against reconstruction bugs in the inverse-DCT step.
TEST_F(FirpmTest, OutputIsSymmetric)
{
    engine.eval("b = firpm(30, [0 0.4 0.5 1], [1 1 0 0]);");
    engine.eval("d = max(abs(b - b(end:-1:1)));");
    EXPECT_LT(eval_scalar("d"), 1e-15);
}
