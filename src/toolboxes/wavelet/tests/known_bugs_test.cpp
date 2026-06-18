// toolboxes/wavelet/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/wavelet/*.md. Disabled until
// fixed; remove `DISABLED_` to turn into a live regression guard.
// MATLAB R2025b reference values.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WaveletKnownBug : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// bugs/wavelet/wentropy-ddencmp.md — wentropy (Shannon).
TEST_F(WaveletKnownBug, DISABLED_WentropyShannon)
{
    EXPECT_NEAR(evalScalar("wentropy([1 2 3 4], 'shannon')"), -69.681618, 1e-4);
}

// bugs/wavelet/wentropy-ddencmp.md — ddencmp default denoise params.
TEST_F(WaveletKnownBug, DISABLED_Ddencmp)
{
    eval("[thr, sorh, keepapp] = ddencmp('den', 'wv', [1 2 3 8 3 2 1 2]);");
    EXPECT_NEAR(evalScalar("thr"), 2.137920, 1e-5);
    EXPECT_DOUBLE_EQ(evalScalar("keepapp"), 1.0);
}

// bugs/wavelet/dwt-biorthogonal.md — bior2.2 analysis coefficients.
TEST_F(WaveletKnownBug, DISABLED_DwtBiorthogonal)
{
    eval("[a, d] = dwt([1 2 3 4 5 6 7 8], 'bior2.2');");
    EXPECT_NEAR(evalScalar("a(1)"), 2.651650, 1e-5);
    EXPECT_NEAR(evalScalar("a(2)"), 1.237437, 1e-5);
}

// bugs/wavelet/wpdec.md — wavelet packet decomposition exists.
// (Needs a tree type; verify node coefficients vs MATLAB when enabling.)
TEST_F(WaveletKnownBug, DISABLED_WpdecExists)
{
    EXPECT_NO_THROW(eval("t = wpdec([1 2 3 4 5 6 7 8], 2, 'db1');"));
}

// bugs/wavelet/wenergy.md — energy distribution of a decomposition.
// FIXED 2026-06-19 (band energy percentages) — promoted live.
TEST_F(WaveletKnownBug, Wenergy)
{
    eval("[c, l] = wavedec([1 2 3 4 5 6 7 8], 2, 'db1'); [Ea, Ed] = wenergy(c, l);");
    EXPECT_NEAR(evalScalar("Ea"), 95.0980392157, 1e-6);
    EXPECT_NEAR(evalScalar("Ea + sum(Ed)"), 100.0, 1e-6);   // percentages sum to 100
}

// bugs/wavelet/upcoef.md — single-branch coefficient reconstruction.
// FIXED 2026-06-19 (synthesis cascade) — promoted live.
TEST_F(WaveletKnownBug, Upcoef)
{
    eval("y = upcoef('a', 5, 'db1', 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 4);
    EXPECT_NEAR(evalScalar("y(1)"), 2.5, 1e-9);
}

// bugs/wavelet/cwt.md — continuous wavelet transform (complex coeff matrix).
TEST_F(WaveletKnownBug, DISABLED_Cwt)
{
    eval("cfs = cwt([1 2 3 4 5 6 7 8 7 6 5 4 3 2 1 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(cfs,2)")), 16);  // time dim == N
    EXPECT_GT(evalScalar("size(cfs,1)"), 1.0);                   // multiple scales
    EXPECT_TRUE(eval("~isreal(cfs)").toBool());                  // complex (Morse)
}

// bugs/wavelet/wavedec2-family.md — 2-D multilevel decomposition + extractors.
TEST_F(WaveletKnownBug, DISABLED_Wavedec2Family)
{
    eval("[c, s] = wavedec2(reshape(1:16,4,4), 1, 'db1');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 16);
    EXPECT_NEAR(evalScalar("c(1)"), 7.0, 1e-9);                 // approx coeff
    eval("H = detcoef2('h', c, s, 1);");
    EXPECT_NEAR(evalScalar("H(1,1)"), -1.0, 1e-9);
    eval("A = appcoef2(c, s, 'db1', 1);");
    EXPECT_NEAR(evalScalar("A(1,1)"), 7.0, 1e-9);
}

// bugs/wavelet/centfrq-scal2frq.md — wavelet center frequency + scale mapping.
TEST_F(WaveletKnownBug, DISABLED_CentfrqScal2frq)
{
    EXPECT_NEAR(evalScalar("centfrq('db4')"),        0.714285714285714, 1e-9);  // 5/7
    EXPECT_NEAR(evalScalar("scal2frq(4, 'db4', 1)"), 0.178571428571429, 1e-9);  // /(a·Δ)
}
