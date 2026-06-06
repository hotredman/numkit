// libs/signal/tests/fir2_test.cpp
// gtest coverage for fir2 — frequency-sampling FIR filter design.
// fir2 is a clean-room implementation from public references (Oppenheim
// & Schafer §7.4-7.5; Rabiner & Gold 1975; Parks & Burrus 1987 — see
//). Hardcoded coefficient values are MATLAB
// R2025b reference output. The property test at the end verifies the
// design MATLAB-independently (the realised frequency response matches
// the request, and the filter is linear-phase / symmetric).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Fir2Test : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Fir2Test, LowpassBitEqual)
{
    eval("b = fir2(20, [0 0.4 0.5 1], [1 1 0 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 21);
    // Peak at center (impulse-response shifted to (N-1)/2 = 10 → 1-based 11).
    EXPECT_NEAR(evalScalar("b(11)"), 0.449219, 1e-5);
    EXPECT_NEAR(evalScalar("b(10)"), 0.305990, 1e-5);
    EXPECT_NEAR(evalScalar("b(12)"), 0.305990, 1e-5);
    // Symmetric (linear-phase FIR).
    EXPECT_NEAR(evalScalar("b(1)"), evalScalar("b(end)"), 1e-12);
    EXPECT_NEAR(evalScalar("b(1)"), 0.001659, 1e-5);
}

TEST_F(Fir2Test, BandpassMultiband)
{
    eval("b = fir2(30, [0 0.2 0.3 0.6 0.7 1], [0 0 1 1 0 0]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 31);
    EXPECT_NEAR(evalScalar("b(16)"), 0.401367, 1e-5);
    // First and last symmetric.
    EXPECT_NEAR(evalScalar("b(1)"), evalScalar("b(end)"), 1e-12);
}

TEST_F(Fir2Test, HighpassBitEqual)
{
    eval("b = fir2(20, [0 0.5 0.6 1], [0 0 1 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 21);
    EXPECT_NEAR(evalScalar("b(11)"), 0.451172, 1e-5);
    EXPECT_NEAR(evalScalar("b(1)"),  0.001658, 1e-5);
}

TEST_F(Fir2Test, RejectsBadFEdges)
{
    bool threw = false;
    try { eval("fir2(10, [0.1 0.5 1], [1 1 0]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// ── Full MATLAB argument set ──────────────────────────────────────────

// Explicit npt grid size. Values are MATLAB R2025b reference output.
TEST_F(Fir2Test, ExplicitNptArgument)
{
    eval("b = fir2(20, [0 0.5 1], [1 1 0], 256);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 21);
    EXPECT_NEAR(evalScalar("sum(b)"), 0.999703451682, 1e-9);
    EXPECT_NEAR(evalScalar("b(9)"),  -0.0924245360805, 1e-9);
    EXPECT_NEAR(evalScalar("b(11)"),  0.75, 1e-12);
}

// Custom output window (Hann instead of the default Hamming).
TEST_F(Fir2Test, CustomWindowArgument)
{
    eval("b = fir2(20, [0 0.5 1], [1 1 0], hann(21));");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 21);
    EXPECT_NEAR(evalScalar("sum(b)"), 0.999979106401, 1e-9);
    EXPECT_NEAR(evalScalar("b(9)"),  -0.0916470217003, 1e-9);
    // A wrong-length window is rejected.
    bool threw = false;
    try { eval("fir2(20, [0 0.5 1], [1 1 0], hann(10));"); }
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

// lap argument — smoothing width at a duplicated (discontinuous) break
// frequency. MATLAB R2025b reference output.
TEST_F(Fir2Test, LapArgumentAtDiscontinuity)
{
    eval("b = fir2(40, [0 0.3 0.3 0.6 0.6 1], [0 0 1 1 0 0], 512, 30);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 41);
    EXPECT_NEAR(evalScalar("b(19)"), -0.238094103298, 1e-9);
    EXPECT_NEAR(evalScalar("b(21)"),  0.30078125,      1e-9);
}

// Odd order with a non-zero response at Nyquist: a symmetric FIR filter
// of odd order has a forced zero at Nyquist, so fir2 bumps the order by
// one. fir2(11, ...) with m(end)=1 therefore returns a length-13 filter.
TEST_F(Fir2Test, OddOrderNyquistCorrection)
{
    eval("b = fir2(11, [0 1], [0 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(b)")), 13);   // 11 → 12
    EXPECT_NEAR(evalScalar("sum(b)"), 0.093077144634, 1e-9);
    EXPECT_NEAR(evalScalar("b(6)"),  -0.190154456393, 1e-9);
    // Even order with non-zero Nyquist is left alone.
    eval("be = fir2(10, [0 1], [0 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(be)")), 11);
}

// ── MATLAB-independent correctness test ───────────────────────────────
// Verify the design actually realises the requested response: build a
// lowpass filter, then evaluate its magnitude response by FFT of the
// zero-padded coefficients. The passband must be ≈ 1 and the stopband
// ≈ 0, and the filter must be symmetric (linear phase).
TEST_F(Fir2Test, RealisesRequestedResponse)
{
    // Lowpass: flat to 0.4, zero from 0.5 (normalised, 1 = Nyquist).
    eval("b = fir2(80, [0 0.4 0.5 1], [1 1 0 0]);\n"
         "nfft = 1024;\n"
         "H = abs(fft(b, nfft));\n"
         "H = H(1:nfft/2+1);\n"           // DC..Nyquist
         "wn = (0:nfft/2) / (nfft/2);\n"  // normalised freq of each bin
         "passMax = max(H(wn <= 0.35));\n"
         "passMin = min(H(wn <= 0.35));\n"
         "stopMax = max(H(wn >= 0.55));");

    // Passband gain is close to unity.
    EXPECT_NEAR(evalScalar("passMax"), 1.0, 0.05);
    EXPECT_NEAR(evalScalar("passMin"), 1.0, 0.05);
    // Stopband is strongly attenuated.
    EXPECT_LT(evalScalar("stopMax"), 0.05);

    // Linear phase ⇒ symmetric impulse response.
    eval("sym_err = max(abs(b - fliplr(b)));");
    EXPECT_LT(evalScalar("sym_err"), 1e-12);
}
