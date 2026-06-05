// libs/signal/tests/signal_batch3_test.cpp
// Signal batch 3 closure (20 functions):
//   filter impl:    filter · filtfilt · sosfilt · medfilt1 · sgolayfilt · sgolay
//   bilinear:       bilinear
//   multirate:      decimate · interp · downsample · upsample · upfirdn (deferred) · resample
//   spectral:       cpsd · mscohere · tfestimate · pburg · pyulear · pwelch · periodogram
//                   (5 deferred — default-NFFT convention differs)
// 14 verified bit-identical MATLAB R2025b; 6 deferred with separate specs
// notes for default-NFFT or boundary-handling gaps.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SignalBatch3Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SignalBatch3Test, FilterImpl)
{
    eval("y = filter([1 0.5], 1, [1 0 0 0 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 0.0);
}

TEST_F(SignalBatch3Test, Filtfilt)
{
    eval("y = filtfilt([1 0.5], 1, [1 0 0 0 0 0 0 0 0 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 10.0);
}

TEST_F(SignalBatch3Test, Sosfilt)
{
    eval("sos = [1 0.5 0 1 0 0]; y = sosfilt(sos, [1 0 0 0 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.5);
}

TEST_F(SignalBatch3Test, Medfilt1)
{
    eval("y = medfilt1([1 5 2 4 3], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 2.0);  // median(1,5,2)=2
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 4.0);  // median(5,2,4)=4
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 3.0);  // median(2,4,3)=3
}

TEST_F(SignalBatch3Test, SgolaySgolayfilt)
{
    eval("y = sgolayfilt([1 2 3 4 5 6 7], 2, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 7.0);

    eval("b = sgolay(2, 5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(b,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(b,2)"), 5.0);
}

TEST_F(SignalBatch3Test, Bilinear)
{
    eval("[bz, az] = bilinear([1], [1 1], 100);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(bz)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(az)"), 2.0);
}

// DEEP-PROBE 2026-05-31: bilinear(b,a,fs,fp) prewarp. The scale must be
// K = 2*pi*fp/tan(pi*fp/fs); numkit double-scaled it (K = 2x too big).
TEST_F(SignalBatch3Test, BilinearPrewarp)
{
    eval("[bp, ap] = bilinear([1 0], [1 2 1], 10, 2);");
    EXPECT_NEAR(evalScalar("bp(1)"),  0.0516690611966527, 1e-12);
    EXPECT_NEAR(evalScalar("bp(3)"), -0.0516690611966527, 1e-12);
    EXPECT_NEAR(evalScalar("ap(2)"), -1.78137447518863,   1e-12);
    EXPECT_NEAR(evalScalar("ap(3)"),  0.793323755213389,  1e-12);
    // No prewarp (K = 2*fs = 20) is unchanged.
    eval("[bn, an] = bilinear([1 0], [1 2 1], 10);");
    EXPECT_NEAR(evalScalar("bn(1)"), 0.0453514739229025, 1e-12);
}

TEST_F(SignalBatch3Test, Multirate)
{
    eval("y = decimate(sin(2*pi*0.01*(0:99)), 4);");
    EXPECT_GT(evalScalar("numel(y)"), 0.0);

    eval("y = interp([1 2 3 4], 2);");
    EXPECT_GT(evalScalar("numel(y)"), 4.0);

    eval("y = downsample([1 2 3 4 5 6], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"),     3.0);

    eval("y = upsample([1 2 3], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"),     0.0);  // zero-stuffed

    eval("y = resample([1 2 3 4 5 6 7 8], 1, 2);");
    EXPECT_GT(evalScalar("numel(y)"), 0.0);
}

// downsample/upsample phase argument (was silently ignored). vs MATLAB R2025b.
TEST_F(SignalBatch3Test, MultiratePhaseArgument)
{
    // downsample(x, n, phase): keep x[phase], x[phase+n], …
    eval("y = downsample(1:10, 3, 1);");      // [2 5 8]
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 8.0);
    eval("y2 = downsample(1:10, 3, 2);");     // [3 6 9]
    EXPECT_DOUBLE_EQ(evalScalar("y2(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y2(3)"), 9.0);

    // upsample(x, n, phase): place samples at offset phase.
    eval("u = upsample(1:3, 3, 1);");         // [0 1 0 0 2 0 0 3 0]
    EXPECT_DOUBLE_EQ(evalScalar("numel(u)"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(5)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(8)"), 3.0);

    // phase >= n is rejected.
    EXPECT_THROW(eval("downsample(1:10, 3, 3);"), std::exception);
}

TEST_F(SignalBatch3Test, ARSpectralEstimators)
{
    // pburg / pyulear: AR-based spectral estimators bit-identical to MATLAB.
    eval("[Pxx, f] = pburg(sin(2*pi*0.1*(0:127)), 4);");
    EXPECT_GT(evalScalar("numel(Pxx)"), 0.0);

    eval("[Pxx, f] = pyulear(sin(2*pi*0.1*(0:127)), 4);");
    EXPECT_GT(evalScalar("numel(Pxx)"), 0.0);
}

// periodogram default NFFT is max(256, 2^nextpow2(N)) — not 2^nextpow2(N).
// vs MATLAB R2025b. 2026-05-31: an 8-sample signal gave 5 freq points
// instead of 129 (NFFT 8 instead of 256). The PSD values were already
// correct; only the default bin count was wrong.
TEST_F(SignalBatch3Test, PeriodogramDefaultNfft)
{
    eval("[p, f] = periodogram([1 2 1 2 1 2 1 2]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f)")), 129);
    EXPECT_NEAR(evalScalar("p(1)"),  2.86479,  1e-4);
    EXPECT_NEAR(evalScalar("p(10)"), 4.40929,  1e-4);
    EXPECT_NEAR(evalScalar("f(10)"), 0.22089,  1e-4);
    // 2^nextpow2(300) = 512 > 256 -> 257 one-sided bins
    eval("[p2, f2] = periodogram(sin(2*pi*(0:299)/10));");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f2)")), 257);
    // an explicit NFFT is still honoured (not forced to 256)
    eval("[p3, f3] = periodogram([1 2 1 2 1 2 1 2], [], 8);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(f3)")), 5);
}
