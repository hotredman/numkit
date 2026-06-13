// toolboxes/signal/tests/spectral_measurements_test.cpp
//
// Coverage for the parity-only spectral-measurement family:
//   meanfreq medfreq powerbw                                (frequency stats)
//   snr sinad thd sfdr                                      (distortion metrics)
//   spectralcrest spectralentropy spectralflatness
//   spectralkurtosis spectralskewness                       (spectral shape)
//   instbw envspectrum                                      (instantaneous / envelope)
//   tsa tachorpm                                            (rotating machinery)
// Driven by a 100 Hz tone with two small harmonics (Fs = 1 kHz). Assertions
// are grounded in the signal physics (centre freq ~100 Hz, SINAD ~ -THD since
// no noise, near-zero flatness for a tonal spectrum) rather than fragile
// decimals. Engine-verified.

#include "dual_engine_fixture.hpp"

using namespace m_test;

class SpectralMeasurementsTest : public DualEngineTest
{
protected:
    void SetUp() override
    {
        DualEngineTest::SetUp();
        eval("Fs = 1000; t = (0:999)'/Fs;");
        eval("x = sin(2*pi*100*t) + 0.05*sin(2*pi*200*t) + 0.01*sin(2*pi*300*t);");
    }
};

TEST_P(SpectralMeasurementsTest, FrequencyStats)
{
    EXPECT_NEAR(evalScalar("meanfreq(x, Fs)"), 100.0, 1.5);   // tone at 100 Hz
    EXPECT_NEAR(evalScalar("medfreq(x, Fs)"), 100.0, 1.5);
    EXPECT_GT(evalScalar("powerbw(x, Fs)"), 0.0);
}

TEST_P(SpectralMeasurementsTest, DistortionMetrics)
{
    EXPECT_GT(evalScalar("snr(x, Fs)"), 50.0);               // no additive noise
    // SINAD (dB) ~ -THD (dB): both dominated by the same harmonics.
    EXPECT_NEAR(evalScalar("sinad(x, Fs)"), -evalScalar("thd(x, Fs)"), 1.0);
    EXPECT_NEAR(evalScalar("sinad(x, Fs)"), 25.85, 1.0);
    EXPECT_NEAR(evalScalar("sfdr(x, Fs)"), 26.0, 1.0);       // 20*log10(1/0.05)
}

TEST_P(SpectralMeasurementsTest, SpectralShape)
{
    EXPECT_LT(evalScalar("spectralflatness(x, Fs)"), 0.01);  // tonal -> near 0
    EXPECT_GT(evalScalar("spectralcrest(x, Fs)"), 10.0);     // sharp peak
    eval("se = spectralentropy(x, Fs);");
    EXPECT_GT(evalScalar("se"), 0.0);
    EXPECT_LT(evalScalar("se"), 1.0);
    EXPECT_GT(evalScalar("spectralkurtosis(x, Fs)"), 0.0);
    EXPECT_TRUE(eval("isfinite(spectralskewness(x, Fs))").toBool());
}

TEST_P(SpectralMeasurementsTest, InstbwEnvspectrum)
{
    EXPECT_GT(eval("instbw(x, Fs)").numel(), 1u);
    EXPECT_DOUBLE_EQ(evalScalar("all(isfinite(instbw(x, Fs)))"), 1.0);
    eval("[es, ef] = envspectrum(x, Fs);");
    EXPECT_GT(eval("es").numel(), 1u);
    EXPECT_EQ(eval("es").numel(), eval("ef").numel());
}

TEST_P(SpectralMeasurementsTest, TsaTachorpm)
{
    // Time-synchronous average of three identical periods returns one period.
    eval("ta = tsa(repmat([1 2 3 4 5 6 7 8 9 10]', 3, 1), Fs, (0:0.01:0.29)');");
    EXPECT_GT(eval("ta").numel(), 1u);
    EXPECT_DOUBLE_EQ(evalScalar("all(isfinite(ta))"), 1.0);
    // Tacho pulse times -> rpm estimate.
    EXPECT_GT(eval("tachorpm([0.1 0.2 0.3 0.4]', Fs)").numel(), 0u);
}

INSTANTIATE_DUAL(SpectralMeasurementsTest);
