// toolboxes/comm/tests/snr_ber_metrics_test.cpp
//
// Coverage for the parity-only SNR / BER / noise utility functions:
//   qfunc qfuncinv marcumq        (special functions)
//   berawgn berconfint            (BER analysis)
//   awgn wgn bsc                  (channels)
//   convertSNR modnorm noisebw    (SNR / normalization)
// Deterministic functions are pinned to closed-form / engine-verified values;
// the random channels (awgn/wgn) are checked by output shape + that they
// actually perturb the signal. DualEngineTest (TW + VM).

#include "dual_engine_fixture.hpp"

using namespace m_test;

class SnrBerMetricsTest : public DualEngineTest
{};

// ── qfunc / qfuncinv / marcumq ──────────────────────────────────────────
TEST_P(SnrBerMetricsTest, QfuncMarcumq)
{
    EXPECT_NEAR(evalScalar("qfunc(0)"), 0.5, 1e-12);
    EXPECT_NEAR(evalScalar("qfunc(1)"), 0.1586552539, 1e-9);
    EXPECT_NEAR(evalScalar("qfuncinv(0.5)"), 0.0, 1e-9);
    EXPECT_NEAR(evalScalar("qfuncinv(qfunc(1))"), 1.0, 1e-7);   // round-trip
    EXPECT_NEAR(evalScalar("marcumq(0, 1)"), 0.6065306597, 1e-9);  // exp(-1/2)
    EXPECT_NEAR(evalScalar("marcumq(1, 2)"), 0.2690126565, 1e-9);
}

// ── berawgn: BPSK BER at Eb/N0=10 dB = qfunc(sqrt(2*10)) ─────────────────
TEST_P(SnrBerMetricsTest, Berawgn)
{
    EXPECT_NEAR(evalScalar("berawgn(10, 'psk', 2, 'nondiff')"), 3.872108e-06, 1e-10);
    EXPECT_NEAR(evalScalar("berawgn(10, 'psk', 2, 'nondiff')"),
                evalScalar("qfunc(sqrt(2 * 10^(10/10)))"), 1e-12);  // closed form
}

// ── berconfint: ber estimate + confidence interval ──────────────────────
TEST_P(SnrBerMetricsTest, Berconfint)
{
    eval("[ber, ci] = berconfint(10, 1000);");
    EXPECT_NEAR(evalScalar("ber"), 0.01, 1e-12);                // 10/1000
    EXPECT_EQ(eval("ci").numel(), 2u);
    EXPECT_NEAR(evalScalar("ci(1)"), 4.805511e-03, 1e-7);
    EXPECT_NEAR(evalScalar("ci(2)"), 1.831324e-02, 1e-7);
    EXPECT_LT(evalScalar("ci(1)"), evalScalar("ber"));
    EXPECT_GT(evalScalar("ci(2)"), evalScalar("ber"));
}

// ── convertSNR / modnorm / noisebw ──────────────────────────────────────
TEST_P(SnrBerMetricsTest, SnrConversionsAndNorms)
{
    // Eb/N0 -> SNR adds 10*log10(bitsPerSymbol): 10 + 10*log10(2) = 13.0103 dB.
    EXPECT_NEAR(evalScalar("convertSNR(10, 'ebno', 'snr', 'BitsPerSymbol', 2)"),
                13.010299957, 1e-6);
    // QPSK constellation has avg power 2 -> scale 1/sqrt(2) for unit avg power.
    EXPECT_NEAR(evalScalar("modnorm([1+1i -1-1i 1-1i -1+1i], 'avpow', 1)"),
                0.70710678, 1e-7);
    EXPECT_NEAR(evalScalar("noisebw(1, 1, 100, 1)"), 1.0, 1e-9);
}

// ── bsc: binary symmetric channel; p=0 is the identity ──────────────────
TEST_P(SnrBerMetricsTest, BscIdentityAtZeroP)
{
    eval("y = bsc([0 1 0 1 1], 0);");
    EXPECT_EQ(eval("y").numel(), 5u);
    EXPECT_DOUBLE_EQ(evalScalar("sum(y == [0 1 0 1 1])"), 5.0);  // no flips
}

// ── awgn / wgn: shape + the channel actually adds noise ─────────────────
TEST_P(SnrBerMetricsTest, AwgnWgnShape)
{
    eval("rng(0); a = awgn([1 1 1 1 1 1 1 1], 10);");
    EXPECT_EQ(eval("a").numel(), 8u);
    EXPECT_GT(evalScalar("norm(a - [1 1 1 1 1 1 1 1])"), 0.0);   // noise added
    eval("rng(0); w = wgn(1, 16, 0);");
    EXPECT_EQ(eval("w").numel(), 16u);
    EXPECT_DOUBLE_EQ(evalScalar("all(isfinite(w))"), 1.0);
}

INSTANTIATE_DUAL(SnrBerMetricsTest);
