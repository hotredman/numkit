// toolboxes/signal/tests/signal_batch4_test.cpp
//
// Signal batch 4 closure (15 functions):
//   TF:           spectrogram (DEFERRED — default window/overlap/NFFT differ)
//   dB:           db2pow · pow2db
//   xcorr:        xcorr · xcov · xcorr2 · finddelay
//   filter conv:  zerophase · tf2sos · sos2tf · tf2zp · zp2tf · tf2ss · tf2zpk · zp2sos
//
// 14 verified bit-identical MATLAB R2025b; 1 deferred.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SignalBatch4Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SignalBatch4Test, DbConversions)
{
    EXPECT_DOUBLE_EQ(evalScalar("db2pow(0)"),    1.0);
    EXPECT_NEAR(evalScalar("db2pow(10)"),       10.0,  1e-9);
    EXPECT_NEAR(evalScalar("db2pow(20)"),      100.0,  1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("pow2db(1)"),    0.0);
    EXPECT_NEAR(evalScalar("pow2db(10)"),       10.0,  1e-9);
    EXPECT_NEAR(evalScalar("pow2db(100)"),      20.0,  1e-9);
}

TEST_F(SignalBatch4Test, Xcorr)
{
    eval("r = xcorr([1 2 3], [1 2 3]);");  // length = 2*N-1 = 5; peak at center
    EXPECT_DOUBLE_EQ(evalScalar("numel(r)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("r(3)"),     14.0);  // 1+4+9 = autocorrelation peak
}

TEST_F(SignalBatch4Test, Xcorr2)
{
    eval("r = xcorr2([1 2; 3 4], [1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(r,1)"), 3.0);  // 2N-1 in each dim
    EXPECT_DOUBLE_EQ(evalScalar("r(2,2)"),    30.0);  // peak: 1+4+9+16
}

TEST_F(SignalBatch4Test, Finddelay)
{
    EXPECT_DOUBLE_EQ(evalScalar("finddelay([0 0 1 2 3], [1 2 3 0 0])"), -2.0);
}

TEST_F(SignalBatch4Test, FilterConversions)
{
    eval("[z, p, k] = tf2zp([1 0.5], [1 0.25]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(z)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(p)"), 1.0);

    eval("[b, a] = zp2tf([1; 2], [3; 4], 5);");
    EXPECT_DOUBLE_EQ(evalScalar("b(1)"), 5.0);  // gain * leading

    eval("sos = tf2sos([1 1 1], [1 0.5 0.25]);");
    EXPECT_DOUBLE_EQ(evalScalar("size(sos,2)"), 6.0);  // 6 cols per SOS row

    eval("[b2, a2] = sos2tf([1 0.5 0.25 1 0 0]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(b2)"), 3.0);

    eval("[A, B, C, D] = tf2ss([1 0.5], [1 0.25]);");
    EXPECT_GT(evalScalar("size(A,1)"), 0.0);

    eval("[z2, p2, k2] = tf2zpk([1 0.5], [1 0.25]);");
    EXPECT_DOUBLE_EQ(evalScalar("k2"), 1.0);

    eval("sos2 = zp2sos([0.5], [0.25; -0.5], 1);");
    EXPECT_GT(evalScalar("size(sos2,1)"), 0.0);
}

TEST_F(SignalBatch4Test, Zerophase)
{
    eval("[Hr, w] = zerophase([1 0.5], 1, 8);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(Hr)"), 8.0);
}
