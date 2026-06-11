// toolboxes/signal/tests/stft_test.cpp
//
// gtest coverage for stft / istft. Fingerprints captured from MATLAB
// R2025b — `sin(2π·0.05·n)` over 512 samples with a hann(64, 'periodic')
// analysis window, 50% overlap, FFTLength 64.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class StftTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
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

// 'centered' FrequencyRange — bit-exact match with MATLAB's MATLAB R2019b+
// default. Rotation: Sd[k] = Fd[(k + cShift) mod N] with cShift = N/2 + 1
// for even N, (N+1)/2 for odd. DC lands at index (N/2 - 1) for even N.
// Values pinned against MATLAB R2025b probe.
TEST_F(StftTest, CenteredMatchesMatlab)
{
    engine.eval("s = stft(x, 'Window', w, 'OverlapLength', 32, "
                "'FFTLength', 64, 'FrequencyRange', 'centered');");
    EXPECT_NEAR(eval_scalar("real(s(1, 1))"),     6.83738e-5, 1e-9);
    EXPECT_NEAR(eval_scalar("imag(s(1, 1))"),     3.11641e-5, 1e-9);
    EXPECT_NEAR(eval_scalar("real(s(33, 1))"),   -0.233456,   1e-5);
    EXPECT_NEAR(eval_scalar("imag(s(33, 1))"),    0.252025,   1e-5);
}

// Round-trip via istft with centered range.
TEST_F(StftTest, IstftRoundTripCentered)
{
    engine.eval("s = stft(x, 'Window', w, 'OverlapLength', 32, "
                "'FFTLength', 64, 'FrequencyRange', 'centered');");
    engine.eval("xr = real(istft(s, 'Window', w, 'OverlapLength', 32, "
                "'FFTLength', 64, 'FrequencyRange', 'centered'));");
    engine.eval("err = max(abs(x(64:end-64) - xr(64:end-64)'));");
    EXPECT_LT(eval_scalar("err"), 1e-12);
}

// Unknown name-value key — must throw rather than silently accept.
TEST_F(StftTest, UnknownNVKeyThrows)
{
    EXPECT_THROW(engine.eval("stft(x, 'BogusKey', 5);"),
                 std::exception);
}

// ── cycle 86: [s, f, t] multi-output + optional fs positional ─────────

// stft(x, fs) with [s, f, t]: f in Hz, t in seconds. twosided range.
TEST_F(StftTest, MultiOutputFsTwosided)
{
    engine.eval("[s, f, t] = stft(x, 1000, 'Window', w, "
                "'OverlapLength', 32, 'FFTLength', 64, "
                "'FrequencyRange', 'twosided');");
    EXPECT_DOUBLE_EQ(eval_scalar("length(f)"), 64.0);
    EXPECT_NEAR(eval_scalar("f(1)"),       0.0,        1e-12);
    EXPECT_NEAR(eval_scalar("f(2)"),      15.625,      1e-12);  // fs/NFFT
    EXPECT_NEAR(eval_scalar("f(end)"),   984.375,      1e-12);
    EXPECT_DOUBLE_EQ(eval_scalar("length(t)"), 15.0);
    EXPECT_NEAR(eval_scalar("t(1)"),       0.032,      1e-12);  // (M/2)/fs
    EXPECT_NEAR(eval_scalar("t(end)"),     0.480,      1e-12);
}

// Centered f axis with fs: even N=64 → [-N/2+1, ..., N/2]·fs/N.
TEST_F(StftTest, MultiOutputFsCentered)
{
    engine.eval("[s, f, t] = stft(x, 1000, 'Window', w, "
                "'OverlapLength', 32, 'FFTLength', 64, "
                "'FrequencyRange', 'centered');");
    EXPECT_DOUBLE_EQ(eval_scalar("length(f)"), 64.0);
    EXPECT_NEAR(eval_scalar("f(1)"),     -484.375, 1e-12);
    EXPECT_NEAR(eval_scalar("f(end)"),    500.0,   1e-12);  // Nyquist at end
    // DC at index N/2 (1-based: 33) for even N=64.
    EXPECT_NEAR(eval_scalar("f(32)"),      0.0,    1e-12);
}

// Onesided: f length = NFFT/2+1, last bin = Nyquist = fs/2.
TEST_F(StftTest, MultiOutputFsOnesided)
{
    engine.eval("[s, f, t] = stft(x, 1000, 'Window', w, "
                "'OverlapLength', 32, 'FFTLength', 64, "
                "'FrequencyRange', 'onesided');");
    EXPECT_DOUBLE_EQ(eval_scalar("length(f)"), 33.0);
    EXPECT_NEAR(eval_scalar("f(1)"),     0.0,   1e-12);
    EXPECT_NEAR(eval_scalar("f(end)"),   500.0, 1e-12);
}

// No fs → default fs=2π for f (radians/sample), fs_t=1 for t (samples).
TEST_F(StftTest, MultiOutputDefaultFs)
{
    engine.eval("[s, f, t] = stft(x);");
    // Default window = hann(128, periodic), 75% overlap, FFTLength 128.
    EXPECT_DOUBLE_EQ(eval_scalar("length(f)"), 128.0);
    // Centered (default range): even N=128 → bin -63 at idx 1.
    EXPECT_NEAR(eval_scalar("f(1)"), -2.0 * 3.14159265358979323846 * 63.0 / 128.0, 1e-12);
    // t(1) = M/2 / fs_t = 64.
    EXPECT_NEAR(eval_scalar("t(1)"), 64.0, 1e-12);
}

// istft 2-output: [x, t] returns time axis (0:Nout-1)/fs.
TEST_F(StftTest, IstftMultiOutputFs)
{
    engine.eval("s = stft(x, 'Window', w, 'OverlapLength', 32, "
                "'FFTLength', 64, 'FrequencyRange', 'twosided');");
    engine.eval("[xr, tr] = istft(s, 1000, 'Window', w, "
                "'OverlapLength', 32, 'FFTLength', 64, "
                "'FrequencyRange', 'twosided');");
    EXPECT_DOUBLE_EQ(eval_scalar("length(tr)"), 512.0);
    EXPECT_NEAR(eval_scalar("tr(1)"),     0.0,    1e-12);
    EXPECT_NEAR(eval_scalar("tr(2)"),     0.001,  1e-12);  // 1/fs
    EXPECT_NEAR(eval_scalar("tr(end)"),   0.511,  1e-12);
}
