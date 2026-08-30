// toolboxes/comm/tests/analog_demod_test.cpp
//
// Analog phase/frequency demodulators pmdemod / fmdemod (inverses of
// pmmod / fmmod). bugs/comm/analog-demodulators.md. Reference values from
// MATLAB R2025b. (These depend on the length-N hilbert — see
// bugs/signal/hilbert-nonpow2.)

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class AnalogDemodTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
                engine.eval("fs=100; fc=10; t=(0:fs-1)'/fs; m=cos(2*pi*1*t);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// pmdemod inverts pmmod: angle of the down-converted analytic signal / pd.
TEST_F(AnalogDemodTest, PmDemod)
{
    eval("mp = pmdemod(pmmod(m, fc, fs, 2), fc, fs, 2);");
    EXPECT_NEAR(evalScalar("mp(1)"),   0.9999999905, 1e-7);
    EXPECT_NEAR(evalScalar("mp(2)"),   0.9980267084, 1e-7);
    EXPECT_NEAR(evalScalar("mp(50)"), -0.9980267084, 1e-7);
}

// fmdemod inverts fmmod: differentiated unwrapped phase.
TEST_F(AnalogDemodTest, FmDemod)
{
    eval("mf = fmdemod(fmmod(m, fc, fs, 5), fc, fs, 5);");
    EXPECT_NEAR(evalScalar("mf(1)"),   0.0,          1e-9);   // first sample = 0
    EXPECT_NEAR(evalScalar("mf(2)"),   0.9977423314, 1e-7);
    EXPECT_NEAR(evalScalar("mf(3)"),   0.9918940853, 1e-7);
    EXPECT_NEAR(evalScalar("mf(50)"), -0.9976932393, 1e-7);
}

// Row input → row output (orientation preserved).
TEST_F(AnalogDemodTest, RowOrientation)
{
    eval("y = pmmod(m', fc, fs, 2); r = pmdemod(y, fc, fs, 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(r,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(r,2)")), 100);
}

// Validation: Fs < 2*Fc throws.
TEST_F(AnalogDemodTest, NyquistThrows)
{
    EXPECT_THROW(eval("pmdemod([1 2 3], 60, 100, 1);"), std::exception);
    EXPECT_THROW(eval("fmdemod([1 2 3], 60, 100, 1);"), std::exception);
}

// amdemod: coherent detection 2*filtfilt(butter(5,fc*2/fs), y.*cos). The
// interior is machine-exact; edges differ by ~3e-6 (numkit filtfilt edge
// conditions vs MATLAB), so assert on a settled interior region.
TEST_F(AnalogDemodTest, AmDemod)
{
    eval("t2=(0:199)'/fs; m2=cos(2*pi*1*t2); ma=amdemod(ammod(m2,fc,fs),fc,fs);");
    EXPECT_NEAR(evalScalar("ma(100)"), 0.9981621945, 1e-6);   // mid-signal: exact
    EXPECT_NEAR(evalScalar("ma(90)"),  0.7707458966, 1e-5);
    EXPECT_NEAR(evalScalar("ma(110)"), 0.8443144337, 1e-5);
}

// ssbdemod: same coherent detector, recovers the message from either sideband.
TEST_F(AnalogDemodTest, SsbDemod)
{
    eval("t2=(0:199)'/fs; m2=cos(2*pi*1*t2); ms=ssbdemod(ssbmod(m2,fc,fs),fc,fs);");
    EXPECT_NEAR(evalScalar("ms(100)"), 0.9982563125, 1e-6);
    // recovers the message in the interior (filter rolloff ~5e-4).
    EXPECT_LT(evalScalar("max(abs(ms(60:140) - m2(60:140)))"), 2e-3);
}
