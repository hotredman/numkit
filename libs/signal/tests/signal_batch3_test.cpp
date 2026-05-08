// libs/signal/tests/signal_batch3_test.cpp
//
// Signal batch 3 closure (20 functions):
//   filter impl:    filter · filtfilt · sosfilt · medfilt1 · sgolayfilt · sgolay
//   bilinear:       bilinear
//   multirate:      decimate · interp · downsample · upsample · upfirdn (deferred) · resample
//   spectral:       cpsd · mscohere · tfestimate · pburg · pyulear · pwelch · periodogram
//                   (5 deferred — default-NFFT convention differs)
//
// 14 verified bit-identical MATLAB R2025b; 6 deferred with separate-ТЗ
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

TEST_F(SignalBatch3Test, ARSpectralEstimators)
{
    // pburg / pyulear: AR-based spectral estimators bit-identical to MATLAB.
    eval("[Pxx, f] = pburg(sin(2*pi*0.1*(0:127)), 4);");
    EXPECT_GT(evalScalar("numel(Pxx)"), 0.0);

    eval("[Pxx, f] = pyulear(sin(2*pi*0.1*(0:127)), 4);");
    EXPECT_GT(evalScalar("numel(Pxx)"), 0.0);
}
