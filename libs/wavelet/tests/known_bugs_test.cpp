// libs/wavelet/tests/known_bugs_test.cpp
//
// One DISABLED_ test per OPEN bug in bugs/wavelet/*.md. Disabled until
// fixed; remove `DISABLED_` to turn into a live regression guard.
// MATLAB R2025b reference values.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class WaveletKnownBug : public ::testing::Test
{
public:
    Engine engine;
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

// bugs/wavelet/wenergy-upcoef.md — energy distribution + coeff reconstruction.
TEST_F(WaveletKnownBug, DISABLED_WenergyUpcoef)
{
    eval("[c, l] = wavedec([1 2 3 4 5 6 7 8], 2, 'db1'); [Ea, Ed] = wenergy(c, l);");
    EXPECT_NEAR(evalScalar("Ea"), 95.0980392157, 1e-6);
    EXPECT_NEAR(evalScalar("Ea + sum(Ed)"), 100.0, 1e-6);   // percentages sum to 100
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
