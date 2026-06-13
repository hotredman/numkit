// toolboxes/wavelet/tests/dwt2_denoise_test.cpp
//
// Coverage for dwt/dwt2.cpp (dwt2/idwt2) and denoise/denoise.cpp (wthresh,
// wdenoise) — parity-spec only before this. dwt2/idwt2 verified by perfect
// reconstruction (Haar + db2 round-trip, matching dwt2_smoke); wthresh by the
// soft/hard thresholding closed form; wdenoise by leaving a clean signal
// essentially unchanged.

#include "dual_engine_fixture.hpp"

#include <cmath>

using namespace m_test;

class WaveletDwt2DenoiseTest : public DualEngineTest
{};

// ── dwt2 / idwt2: 2-D DWT + perfect reconstruction ──────────

TEST_P(WaveletDwt2DenoiseTest, Dwt2HaarShapeAndRoundTrip)
{
    eval("X = reshape(1:16, 4, 4); [cA, cH, cV, cD] = dwt2(X, 'haar'); "
         "Xr = idwt2(cA, cH, cV, cD, 'haar');");
    EXPECT_EQ(eval("cA").dims().rows(), 2u);   // half size each dim
    EXPECT_EQ(eval("cA").dims().cols(), 2u);
    EXPECT_NEAR(evalScalar("cA(1,1)"), 7.0, 1e-9);
    EXPECT_NEAR(evalScalar("max(max(abs(Xr - X)))"), 0.0, 1e-9);  // perfect reconstruction
}

TEST_P(WaveletDwt2DenoiseTest, Dwt2Db2RoundTrip)
{
    eval("X = reshape(1:16, 4, 4); [cA, cH, cV, cD] = dwt2(X, 'db2'); "
         "Xr = idwt2(cA, cH, cV, cD, 'db2', size(X)); e = max(max(abs(Xr - X)));");
    EXPECT_NEAR(evalScalar("e"), 0.0, 1e-9);
}

// ── wthresh: soft / hard thresholding ───────────────────────

TEST_P(WaveletDwt2DenoiseTest, WthreshSoft)
{
    EXPECT_NEAR(evalScalar("wthresh(2, 's', 1)"), 1.0, 1e-12);    // shrink toward 0
    EXPECT_NEAR(evalScalar("wthresh(-0.5, 's', 1)"), 0.0, 1e-12);  // |x| < t → 0
}

TEST_P(WaveletDwt2DenoiseTest, WthreshHard)
{
    EXPECT_NEAR(evalScalar("wthresh(2, 'h', 1)"), 2.0, 1e-12);    // keep
    EXPECT_NEAR(evalScalar("wthresh(0.5, 'h', 1)"), 0.0, 1e-12);  // |x| < t → 0
}

// ── wdenoise: leaves a clean signal essentially unchanged ───

TEST_P(WaveletDwt2DenoiseTest, WdenoiseCleanSignal)
{
    eval("x = linspace(0, 1, 64); y = wdenoise(x); e = max(abs(y(:) - x(:)));");
    EXPECT_EQ(eval("y").numel(), 64u);
    EXPECT_LT(evalScalar("e"), 1e-6);  // smooth ramp → near-identity
}

INSTANTIATE_DUAL(WaveletDwt2DenoiseTest);
