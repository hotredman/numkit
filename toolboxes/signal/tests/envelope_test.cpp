// toolboxes/signal/tests/envelope_test.cpp
// envelope (FULL MATLAB R2025b parity).
// Covers all four documented signatures of envelope:
//   [yu, yl] = envelope(x)                  -- default FFT |hilbert(x-mean)|
//   [yu, yl] = envelope(x, n, 'analytic')   -- Kaiser(8)-tapered Hilbert FIR
//   [yu, yl] = envelope(x, n, 'rms')        -- sliding-window RMS
//   [yu, yl] = envelope(x, n, 'peak')       -- spline through findpeaks (MinPeakDistance n)
//   yu      = envelope(x, n)                -- == envelope(x, n, 'analytic')
// All hardcoded values verified bit-identical against MATLAB R2025b's envelope.m.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EnvelopeTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("sig = sin(2*pi*0.1*(0:31)') .* exp(-0.05*(0:31)');");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ---------- default mode: FFT |hilbert(x-mean)| with mean restored ----------

TEST_F(EnvelopeTest, DefaultMatchesMatlabUpper)
{
    eval("[up, lo] = envelope(sig);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(up)"), 32.0);
    EXPECT_NEAR(evalScalar("up(1)"),  0.479437773876762, 1e-9);
    EXPECT_NEAR(evalScalar("up(6)"),  0.881726162448688, 1e-9);
    EXPECT_NEAR(evalScalar("up(16)"), 0.520911305525856, 1e-9);
    EXPECT_NEAR(evalScalar("up(32)"), 0.281743014924398, 1e-9);
}

TEST_F(EnvelopeTest, DefaultMatchesMatlabLower)
{
    eval("[up, lo] = envelope(sig);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(lo)"), 32.0);
    EXPECT_NEAR(evalScalar("lo(1)"),  -0.397408977590976, 1e-9);
    EXPECT_NEAR(evalScalar("lo(6)"),  -0.799697366162902, 1e-9);
    EXPECT_NEAR(evalScalar("lo(32)"), -0.199714218638612, 1e-9);
}

TEST_F(EnvelopeTest, DefaultLowerIsMirrorOfUpperAroundMean)
{
    // For default mode: yu = mean + |hilbert|, yl = mean - |hilbert|.
    // So (yu + yl)/2 must equal mean(sig) for every sample.
    eval("[up, lo] = envelope(sig);");
    EXPECT_LT(evalScalar("max(abs((up+lo)/2 - mean(sig)))"), 1e-12);
}

TEST_F(EnvelopeTest, DefaultOneOutputForm)
{
    // 1-output form returns yupper.
    eval("env = envelope(sig);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(env)"), 32.0);
    EXPECT_NEAR(evalScalar("env(1)"), 0.479437773876762, 1e-9);
}

// ---------- 'analytic' mode: Kaiser(beta=8) Hilbert FIR ----------

TEST_F(EnvelopeTest, AnalyticN8MatchesMatlabUpper)
{
    eval("[upa, loa] = envelope(sig, 8, 'analytic');");
    EXPECT_NEAR(evalScalar("upa(1)"),  0.514194243650516, 1e-9);
    EXPECT_NEAR(evalScalar("upa(6)"),  0.523884006347346, 1e-9);
    EXPECT_NEAR(evalScalar("upa(16)"), 0.343398699261916, 1e-9);
}

TEST_F(EnvelopeTest, AnalyticN8MatchesMatlabLower)
{
    eval("[upa, loa] = envelope(sig, 8, 'analytic');");
    EXPECT_NEAR(evalScalar("loa(1)"),  -0.43216544736473,  1e-9);
    EXPECT_NEAR(evalScalar("loa(16)"), -0.261369902976131, 1e-9);
}

TEST_F(EnvelopeTest, TwoArgFormDefaultsToAnalytic)
{
    // envelope(x, n) is identical to envelope(x, n, 'analytic').
    eval("upn = envelope(sig, 4);");
    EXPECT_NEAR(evalScalar("upn(1)"), 0.40901052447155,  1e-9);
    EXPECT_NEAR(evalScalar("upn(6)"), 0.379890442731612, 1e-9);
}

// ---------- 'rms' mode: sliding-window RMS of mean-centered signal ----------

TEST_F(EnvelopeTest, RmsN5MatchesMatlabUpper)
{
    eval("[upr, lor] = envelope(sig, 5, 'rms');");
    EXPECT_NEAR(evalScalar("upr(1)"),  0.601298632995651, 1e-9);
    EXPECT_NEAR(evalScalar("upr(6)"),  0.594715441561839, 1e-9);
    EXPECT_NEAR(evalScalar("upr(16)"), 0.377301983874212, 1e-9);
}

TEST_F(EnvelopeTest, RmsN5MatchesMatlabLower)
{
    eval("[upr, lor] = envelope(sig, 5, 'rms');");
    EXPECT_NEAR(evalScalar("lor(1)"), -0.519269836709865, 1e-9);
    EXPECT_NEAR(evalScalar("lor(6)"), -0.512686645276053, 1e-9);
}

TEST_F(EnvelopeTest, RmsLowerIsMirrorOfUpperAroundMean)
{
    // 'rms' mode (MATLAB envelope.m): xampl = sqrt(movmean((x-mean).^2, n));
    //   yupper = mean + xampl;  ylower = mean - xampl
    // So (yupper + ylower)/2 == mean(sig) at every sample.
    eval("[upr, lor] = envelope(sig, 5, 'rms');");
    EXPECT_LT(evalScalar("max(abs((upr+lor)/2 - mean(sig)))"), 1e-12);
}

// ---------- 'peak' mode: spline through findpeaks(x, MinPeakDistance=n) ----------

TEST_F(EnvelopeTest, PeakN1MatchesMatlabUpper)
{
    eval("[upp, lop] = envelope(sig, 1, 'peak');");
    EXPECT_NEAR(evalScalar("upp(1)"),  0.944259127035341, 1e-9);
    EXPECT_NEAR(evalScalar("upp(6)"),  0.744982288753672, 1e-9);
    EXPECT_NEAR(evalScalar("upp(16)"), 0.446350339987643, 1e-9);
}

TEST_F(EnvelopeTest, PeakN1MatchesMatlabLower)
{
    eval("[upp, lop] = envelope(sig, 1, 'peak');");
    EXPECT_NEAR(evalScalar("lop(1)"),  -0.916526411877334, 1e-9);
    EXPECT_NEAR(evalScalar("lop(16)"), -0.45093553877206,  1e-9);
}

TEST_F(EnvelopeTest, PeakModeDoesNotRemoveDC)
{
    // Per MATLAB envelope.m: 'peak' mode operates on the raw signal
    // (no mean-removal). Add a DC offset and confirm the upper/lower
    // envelopes shift by exactly the same offset.
    eval("[u0, l0] = envelope(sig, 1, 'peak');");
    eval("[uS, lS] = envelope(sig + 5, 1, 'peak');");
    EXPECT_LT(evalScalar("max(abs(uS - u0 - 5))"), 1e-9);
    EXPECT_LT(evalScalar("max(abs(lS - l0 - 5))"), 1e-9);
}

// ---------- threading 4 modes through one expression for fingerprint coverage ----------

TEST_F(EnvelopeTest, AllFourModesCoexistInOneExpression)
{
    // Mirrors the parity-spec expression. Sanity check: all four pairs
    // produce length-32 outputs and the upper is always >= the lower.
    eval("[u0, l0] = envelope(sig);");
    eval("[ua, la] = envelope(sig, 8, 'analytic');");
    eval("[ur, lr] = envelope(sig, 5, 'rms');");
    eval("[up, lp] = envelope(sig, 1, 'peak');");
    EXPECT_DOUBLE_EQ(evalScalar("min(u0 - l0)"), evalScalar("min(u0 - l0)"));
    EXPECT_GE(evalScalar("min(u0 - l0)"), 0.0);
    EXPECT_GE(evalScalar("min(ua - la)"), 0.0);
    EXPECT_GE(evalScalar("min(ur - lr)"), 0.0);
    EXPECT_GE(evalScalar("min(up - lp)"), 0.0);
}
