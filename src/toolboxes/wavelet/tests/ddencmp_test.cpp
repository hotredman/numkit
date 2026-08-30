// toolboxes/wavelet/tests/ddencmp_test.cpp
//
// ddencmp(opt, type, x) — default denoising / compression parameters.
// bugs/wavelet/ddencmp.md. Reference values from MATLAB R2025b.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <string>

using namespace numkit;

class DdencmpTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
    std::string evalStr(const std::string &c) { return eval(c).toString(); }
};

// Denoising: universal threshold sqrt(2*log(n))*sigma, soft, keep approx.
TEST_F(DdencmpTest, DenoiseWavelet)
{
    eval("[thr, sorh, keepapp] = ddencmp('den', 'wv', [1 2 3 8 3 2 1 2]);");
    EXPECT_NEAR(evalScalar("thr"), 2.137919772574, 1e-9);
    EXPECT_EQ(evalStr("sorh"), "s");
    EXPECT_DOUBLE_EQ(evalScalar("keepapp"), 1.0);
}

// Odd-length signal (db1 boundary handling matches MATLAB).
TEST_F(DdencmpTest, DenoiseOddLength)
{
    EXPECT_NEAR(evalScalar("[t,~,~]=ddencmp('den','wv',[1 2 3 4 5]); t"),
                1.880854323469, 1e-9);
}

// Compression: threshold = median(|cD1|), hard, keep approx.
TEST_F(DdencmpTest, CompressWavelet)
{
    eval("[thr, sorh, keepapp] = ddencmp('cmp', 'wv', [1 2 3 8 3 2 1 2]);");
    EXPECT_NEAR(evalScalar("thr"), 0.707106781187, 1e-9);
    EXPECT_EQ(evalStr("sorh"), "h");
    EXPECT_DOUBLE_EQ(evalScalar("keepapp"), 1.0);
}

// Wavelet-packet ('wp') is a documented gap — clear error.
TEST_F(DdencmpTest, WaveletPacketThrows)
{
    EXPECT_THROW(eval("ddencmp('den', 'wp', [1 2 3 4]);"), std::exception);
}
