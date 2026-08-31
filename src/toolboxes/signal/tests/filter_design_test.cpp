// tests/filter_design_test.cpp

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class FilterDesignTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ============================================================
// butter
// ============================================================

TEST_F(FilterDesignTest, ButterReturnsCorrectLength)
{
    // Nth order → N+1 coefficients
    eval("[b, a] = butter(4, 0.5);");
    EXPECT_EQ(eval("b").numel(), 5u);
    EXPECT_EQ(eval("a").numel(), 5u);
}

TEST_F(FilterDesignTest, ButterA0IsOne)
{
    eval("[b, a] = butter(3, 0.3);");
    EXPECT_NEAR(evalScalar("a(1)"), 1.0, 1e-10);
}

TEST_F(FilterDesignTest, ButterLowpassUnityDcGain)
{
    // Lowpass: sum(b)/sum(a) should be ~1 at DC
    eval("[b, a] = butter(4, 0.5);");
    double dcGain = evalScalar("abs(sum(b) / sum(a))");
    EXPECT_NEAR(dcGain, 1.0, 1e-6);
}

TEST_F(FilterDesignTest, ButterHighpass)
{
    eval("[b, a] = butter(4, 0.5, 'high');");
    EXPECT_EQ(eval("b").numel(), 5u);
    // DC gain should be ~0 for highpass
    double dcGain = evalScalar("abs(sum(b) / sum(a))");
    EXPECT_NEAR(dcGain, 0.0, 0.1);
}

TEST_F(FilterDesignTest, ButterHighpassNyquistGain)
{
    // At Nyquist, highpass gain ≈ 1
    eval("[b, a] = butter(4, 0.5, 'high');");
    // Evaluate at z = -1: alternate signs
    eval("bg = 0; ag = 0;");
    eval("for k = 1:length(b); bg = bg + b(k) * (-1)^(k-1); end;");
    eval("for k = 1:length(a); ag = ag + a(k) * (-1)^(k-1); end;");
    double nyqGain = evalScalar("abs(bg / ag)");
    EXPECT_NEAR(nyqGain, 1.0, 0.1);
}

TEST_F(FilterDesignTest, ButterCombinedWithFilter)
{
    // Filter a DC signal with lowpass → should pass through
    eval("[b, a] = butter(2, 0.5);");
    eval("x = ones(1, 100);");
    eval("y = filter(b, a, x);");
    // Steady-state output should approach 1.0
    EXPECT_NEAR(evalScalar("y(100)"), 1.0, 0.01);
}

// ============================================================
// fir1
// ============================================================

TEST_F(FilterDesignTest, Fir1Length)
{
    // fir1(N, Wn) returns N+1 coefficients
    auto r = eval("fir1(20, 0.5)");
    EXPECT_EQ(r.numel(), 21u);
}

TEST_F(FilterDesignTest, Fir1LowpassDcGain)
{
    eval("b = fir1(30, 0.5);");
    double dcGain = evalScalar("sum(b)");
    EXPECT_NEAR(dcGain, 1.0, 0.01);
}

TEST_F(FilterDesignTest, Fir1Symmetric)
{
    // Linear-phase FIR: symmetric coefficients
    eval("b = fir1(20, 0.3);");
    for (int i = 1; i <= 10; ++i) {
        std::string l = "b(" + std::to_string(i) + ")";
        std::string r = "b(" + std::to_string(22 - i) + ")";
        EXPECT_NEAR(evalScalar(l), evalScalar(r), 1e-10);
    }
}

TEST_F(FilterDesignTest, Fir1Highpass)
{
    eval("b = fir1(30, 0.5, 'high');");
    // DC gain should be ~0
    double dcGain = evalScalar("abs(sum(b))");
    EXPECT_NEAR(dcGain, 0.0, 0.01);
}

// ============================================================
// freqz
// ============================================================

TEST_F(FilterDesignTest, FreqzOutputLengths)
{
    eval("[H, W] = freqz([1 1], [1], 128);");
    EXPECT_EQ(eval("H").numel(), 128u);
    EXPECT_EQ(eval("W").numel(), 128u);
}

TEST_F(FilterDesignTest, FreqzFrequencyRange)
{
    // MATLAB freqz(b, a, n) places n equispaced frequencies on
    // [0, π) — upper endpoint π is EXCLUDED. So
    //   W(1)   = 0
    //   W(n)   = (n-1) * π / n        (NOT π)
    eval("[H, W] = freqz([1], [1], 256);");
    EXPECT_NEAR(evalScalar("W(1)"),   0.0,                       1e-10);
    EXPECT_NEAR(evalScalar("W(256)"), 255.0 * M_PI / 256.0,      1e-10);
}

TEST_F(FilterDesignTest, FreqzUnitGainForPassthrough)
{
    // b=[1], a=[1] → H(w) = 1 for all w
    eval("[H, W] = freqz([1], [1], 64);");
    eval("Hmag = abs(H);");
    for (int i = 1; i <= 64; ++i) {
        std::string hi = "Hmag(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(hi), 1.0, 1e-10);
    }
}

TEST_F(FilterDesignTest, FreqzButterAtCutoff)
{
    // At cutoff, Butterworth gain ≈ -3dB ≈ 0.707
    eval("[b, a] = butter(4, 0.5);");
    eval("[H, W] = freqz(b, a, 512);");
    eval("Hmag = abs(H);");
    // Find index closest to Wn*pi = 0.5*pi
    eval("target = 0.5 * pi;");
    eval("[~, idx] = min(abs(W - target));");
    double gainAtCutoff = evalScalar("Hmag(idx)");
    EXPECT_NEAR(gainAtCutoff, 1.0 / std::sqrt(2.0), 0.05);
}

// ============================================================
// phasez
// ============================================================

TEST_F(FilterDesignTest, PhasezPassthroughIsZero)
{
    // b=[1], a=[1] → H(w) = 1 for all w → phase = 0.
    eval("[phi, W] = phasez([1], [1], 64);");
    for (int i = 1; i <= 64; ++i) {
        std::string p = "phi(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(p), 0.0, 1e-12);
    }
}

TEST_F(FilterDesignTest, PhasezReturnsCorrectShape)
{
    // MATLAB phasez(b, a, n): n points on [0, π) — upper endpoint
    // π is EXCLUDED, so W(n) = (n-1) * π / n.
    eval("[phi, W] = phasez([1 -0.5], [1], 128);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(phi)"), 128.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(W)"), 128.0);
    EXPECT_NEAR(evalScalar("W(1)"),   0.0,                  1e-10);
    EXPECT_NEAR(evalScalar("W(128)"), 127.0 * M_PI / 128.0, 1e-10);
}

TEST_F(FilterDesignTest, PhasezPureDelayFilterIsLinear)
{
    // b = [0 0 0 1] (3-sample delay), a = [1] → phase = -3·w (linear, slope = -3).
    eval("[phi, W] = phasez([0 0 0 1], [1], 256);");
    double slope = evalScalar("(phi(200) - phi(50)) / (W(200) - W(50));");
    EXPECT_NEAR(slope, -3.0, 1e-9);
}

TEST_F(FilterDesignTest, PhasezUnwrappedIsContinuous)
{
    // After unwrap, no consecutive jump may exceed pi.
    eval("[phi, W] = phasez([0 0 0 0 0 0 0 0 1], [1], 128);");
    for (int i = 2; i <= 128; ++i) {
        double d = evalScalar("phi(" + std::to_string(i) + ") - phi("
                              + std::to_string(i - 1) + ");");
        EXPECT_LT(std::abs(d), M_PI);
    }
}

// ============================================================
// grpdelay
// ============================================================

TEST_F(FilterDesignTest, GrpdelayPassthroughIsZero)
{
    eval("[gd, W] = grpdelay([1], [1], 64);");
    for (int i = 1; i <= 64; ++i) {
        std::string g = "gd(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(g), 0.0, 1e-9);
    }
}

TEST_F(FilterDesignTest, GrpdelayPureDelayIsConstant)
{
    // 3-sample delay → group delay = 3 samples everywhere.
    eval("[gd, W] = grpdelay([0 0 0 1], [1], 128);");
    for (int i = 1; i <= 128; ++i) {
        double v = evalScalar("gd(" + std::to_string(i) + ");");
        EXPECT_NEAR(v, 3.0, 1e-6);
    }
}

// DEEP-PROBE 2026-05-31: grpdelay now uses the EXACT ramped-polynomial
// method (gd = Re{CR/C} - (na-1), c = conv(b, reverse(a))), NOT a phase
// finite-difference — which was wildly wrong at small npts (gd(0) gave
// 1.137 instead of 1.5 for the filter below) and couldn't represent the
// negative group delays of the second filter at all.
TEST_F(FilterDesignTest, GrpdelayExactMatchesMatlab)
{
    // H = (1 + z^-1)/(1 - 0.5 z^-1): gd(0) = 1.5 (symmetric FIR num + pole).
    eval("[gd, W] = grpdelay([1 1], [1 -0.5], 4);");
    EXPECT_NEAR(evalScalar("gd(1)"), 1.5,               1e-9);
    EXPECT_NEAR(evalScalar("gd(2)"), 0.690743569830546, 1e-9);
    EXPECT_NEAR(evalScalar("gd(3)"), 0.3,               1e-9);
    EXPECT_NEAR(evalScalar("gd(4)"), 0.191609371345924, 1e-9);
    // Filter exhibiting NEGATIVE group delay at low frequencies.
    eval("[gd2, W2] = grpdelay([1 -0.3 0.2], [1 0.4 0.1], 5);");
    EXPECT_NEAR(evalScalar("gd2(1)"), -0.288888888889, 1e-7);
    EXPECT_NEAR(evalScalar("gd2(3)"), -0.656839484500, 1e-7);
    EXPECT_NEAR(evalScalar("gd2(5)"),  0.715501746200, 1e-7);
}

TEST_F(FilterDesignTest, GrpdelayShape)
{
    eval("[gd, W] = grpdelay([1 0.5], [1], 64);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(gd)"), 64.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(W)"),  64.0);
}

// DEEP-PROBE 2026-05-31: phasez/grpdelay(b,a,n,fs) sample-rate form — the
// frequency vector is in Hz over [0, fs/2) (= w*fs/(2*pi)); the phase /
// group-delay values are unchanged. numkit ignored fs and returned radians.
TEST_F(FilterDesignTest, PhasezGrpdelaySampleRate)
{
    eval("[ph, f] = phasez([1 1], [1 -0.5], 4, 100);");
    EXPECT_DOUBLE_EQ(evalScalar("f(1)"), 0.0);
    EXPECT_NEAR(evalScalar("f(2)"), 12.5, 1e-12);
    EXPECT_NEAR(evalScalar("f(4)"), 37.5, 1e-12);
    EXPECT_NEAR(evalScalar("ph(2)"), -0.89317311847, 1e-9);   // phase unchanged
    eval("[gd, fg] = grpdelay([1 1], [1 -0.5], 4, 100);");
    EXPECT_NEAR(evalScalar("fg(2)"), 12.5, 1e-12);
    EXPECT_NEAR(evalScalar("fg(4)"), 37.5, 1e-12);
    EXPECT_NEAR(evalScalar("gd(2)"), 0.690743569830546, 1e-9);  // gd unchanged
    // no fs → frequency stays in radians.
    eval("[~, w] = grpdelay([1 1], [1 -0.5], 4);");
    EXPECT_NEAR(evalScalar("w(2)"), 0.7853981633974483, 1e-12);  // pi/4
}

// --- bugs/closed/signal/butter-analog-flag-wn-domain.md (FIXED) ---
// butter routes through the shared analog/digital pipeline: 's' keeps Wn
// in rad/s. Values below are MATLAB R2025b ground truth (probed 2026-08-31;
// lp coefficients exact to 15 digits, bp within libm noise).
TEST_F(FilterDesignTest, ButterAnalogFlagWnDomain)
{
    eval("[B, A] = butter(4, 100, 's');");
    EXPECT_NEAR(evalScalar("B(5)"), 1e8, 1e-6);
    EXPECT_NEAR(evalScalar("A(2)"), 261.312592975275, 1e-9);
    EXPECT_NEAR(evalScalar("A(5)"), 1e8, 1e-6);
    eval("[B2, A2] = butter(2, [40 150], 's');");
    EXPECT_NEAR(evalScalar("B2(3)"), 12100.0, 1e-9);       // BW^2 gain
    EXPECT_NEAR(evalScalar("A2(2)"), 155.56349186104, 1e-9);
    EXPECT_NEAR(evalScalar("A2(5)"), 3.6e7, 1e-4);
    eval("[B3, A3] = butter(3, 2000, 'high', 's');");
    EXPECT_NEAR(evalScalar("A3(2)"), 4000.0, 1e-9);         // 2*Wn
    EXPECT_NEAR(evalScalar("A3(4)"), 8e9, 1e0);             // Wn^3
    // Digital path unchanged through the new pipeline (MATLAB-probed pin).
    eval("[bd, ad] = butter(4, 0.5);");
    EXPECT_NEAR(evalScalar("bd(5)"), 0.0939808514337944, 1e-15);
    EXPECT_NEAR(evalScalar("ad(5)"), 0.0176648008724419, 1e-15);
}

// --- bugs/closed/signal/freqs-two-arg-auto-w.md (FIXED, freqint-exact) ---
// The grid is the classic freqint algorithm (Grace 1990): decade-rounded
// extremes from upper-half pole/zero roots, long base logspace + refinement
// windows around oscillatory roots, resampled to 200 points. Endpoints are
// bit-exact vs R2025b (probed); interior points match to <=2 ulp (libm
// pow/log10 differ between MSVC and MATLAB's runtime).
TEST_F(FilterDesignTest, FreqsTwoArgAutoW)
{
    // Endpoints bit-exact across every rule branch (probed vs R2025b).
    eval("[~, w] = freqs([1], [1 sqrt(2) 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 0.1);
    EXPECT_DOUBLE_EQ(evalScalar("w(end)"), 10.0);
    eval("[~, w] = freqs([1], [1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 0.01);
    EXPECT_DOUBLE_EQ(evalScalar("w(end)"), 10.0);
    eval("[~, w] = freqs([1], [1 100]);");   // round(0.5)=1 half-away
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(end)"), 1000.0);
    eval("[~, w] = freqs([1], [1 1000]);");
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(end)"), 10000.0);
    eval("[~, w] = freqs(1, 1);");           // no roots -> derived default
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(end)"), 10000.0);
    eval("[~, w] = freqs([1 5], [1 50]);");
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 0.1);
    EXPECT_DOUBLE_EQ(evalScalar("w(end)"), 1000.0);
    // Oscillatory refinement (imaginary zero at 10i): interior densifies
    // near 10; the whole grid resampled to exactly 200 log-spaced-ish points.
    eval("[~, w] = freqs([1 0 100], [1 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"), 0.1);
    EXPECT_DOUBLE_EQ(evalScalar("w(end)"), 100.0);
    EXPECT_NEAR(evalScalar("w(2)"), 0.10371385080926335, 1e-15);
    EXPECT_EQ(eval("numel(w);").toScalar(), 200.0);
    // 2nd-order Butterworth prototype response on the grid: |H(0.1)| =
    // 1/sqrt(1 + 0.1^4) = 0.9999500037496877 (grid starts at 0.1).
    eval("h = freqs([1], [1 sqrt(2) 1]);");
    EXPECT_NEAR(evalScalar("abs(h(1))"), 0.9999500037496877, 1e-12);
    EXPECT_LT(evalScalar("abs(h(end))"), evalScalar("abs(h(100))"));
}
