// libs/signal/tests/stft_test.cpp
//
// gtest coverage for stft / istft. Fingerprints captured from MATLAB
// R2025b — `sin(2π·0.05·n)` over 512 samples with a hann(64, 'periodic')
// analysis window, 50% overlap, FFTLength 64.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class StftTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void   SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("x = sin(2*pi*0.05*(0:511));");
        engine.eval("w = 0.5*(1 - cos(2*pi*(0:63)/64));");  // hann(64,'periodic')
    }
    double eval_scalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// Twosided default — output rows = FFTLength, MATLAB fingerprints.
TEST_F(StftTest, TwosidedMatchesMatlab)
{
    engine.eval("s = stft(x, 'Window', w, 'OverlapLength', 32, "
                "'FFTLength', 64, 'FrequencyRange', 'twosided');");
    EXPECT_DOUBLE_EQ(eval_scalar("size(s, 1)"), 64.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(s, 2)"), 15.0);
    EXPECT_NEAR(eval_scalar("real(s(1, 1))"),    -0.119014, 1e-6);
    EXPECT_NEAR(eval_scalar("real(s(33, 1))"),   6.76931e-5, 1e-9);
    EXPECT_NEAR(eval_scalar("real(s(32, 3))"),   1.10631e-4, 1e-9);
    EXPECT_NEAR(eval_scalar("imag(s(32, 3))"),   1.19036e-5, 1e-9);
}

// Onesided — first NFFT/2+1 rows match the twosided prefix.
TEST_F(StftTest, OnesidedTruncation)
{
    engine.eval("so = stft(x, 'Window', w, 'OverlapLength', 32, "
                "'FFTLength', 64, 'FrequencyRange', 'onesided');");
    EXPECT_DOUBLE_EQ(eval_scalar("size(so, 1)"), 33.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(so, 2)"), 15.0);
    EXPECT_NEAR(eval_scalar("real(so(1, 1))"), -0.119014, 1e-6);
    // Onesided[33] = Nyquist bin = twosided[33] for FFTLength 64.
    EXPECT_NEAR(eval_scalar("real(so(33, 1))"), 6.76931e-5, 1e-9);
}

// Round-trip via istft — COLA-compliant (hann/periodic + 50% overlap)
// reconstructs to ulp precision on interior samples.
TEST_F(StftTest, IstftRoundTripTwosided)
{
    engine.eval("s = stft(x, 'Window', w, 'OverlapLength', 32, "
                "'FFTLength', 64);");
    engine.eval("xr = real(istft(s, 'Window', w, 'OverlapLength', 32, "
                "'FFTLength', 64));");
    EXPECT_DOUBLE_EQ(eval_scalar("length(xr)"), 512.0);
    engine.eval("err = max(abs(x(64:end-64) - xr(64:end-64)'));");
    EXPECT_LT(eval_scalar("err"), 1e-12);
}

// Onesided round-trip — Hermitian mirroring + ifft + overlap-add must
// recover the real input.
TEST_F(StftTest, IstftRoundTripOnesided)
{
    engine.eval("so = stft(x, 'Window', w, 'OverlapLength', 32, "
                "'FFTLength', 64, 'FrequencyRange', 'onesided');");
    engine.eval("xr = real(istft(so, 'Window', w, 'OverlapLength', 32, "
                "'FFTLength', 64, 'FrequencyRange', 'onesided'));");
    engine.eval("err = max(abs(x(64:end-64) - xr(64:end-64)'));");
    EXPECT_LT(eval_scalar("err"), 1e-12);
}

// 'centered' FrequencyRange — deferred; documented throw.
TEST_F(StftTest, CenteredDeferredThrows)
{
    EXPECT_THROW(engine.eval("stft(x, 'Window', w, 'OverlapLength', 32, "
                             "'FFTLength', 64, 'FrequencyRange', "
                             "'centered');"),
                 std::exception);
}

// Unknown name-value key — must throw rather than silently accept.
TEST_F(StftTest, UnknownNVKeyThrows)
{
    EXPECT_THROW(engine.eval("stft(x, 'BogusKey', 5);"),
                 std::exception);
}
