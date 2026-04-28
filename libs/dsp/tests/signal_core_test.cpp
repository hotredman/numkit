// tests/signal_core_test.cpp

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit::m;

class SignalCoreTest : public ::testing::Test
{
public:
    Engine engine;
    std::string capturedOutput;

    void SetUp() override
    {
        StdLibrary::install(engine);
        capturedOutput.clear();
        engine.setOutputFunc([this](const std::string &s) { capturedOutput += s; });
    }

    MValue eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ============================================================
// nextpow2
// ============================================================

TEST_F(SignalCoreTest, Nextpow2PowerOfTwo)
{
    EXPECT_DOUBLE_EQ(evalScalar("nextpow2(8)"), 3.0);
}

TEST_F(SignalCoreTest, Nextpow2NonPower)
{
    EXPECT_DOUBLE_EQ(evalScalar("nextpow2(5)"), 3.0);
}

TEST_F(SignalCoreTest, Nextpow2One)
{
    EXPECT_DOUBLE_EQ(evalScalar("nextpow2(1)"), 0.0);
}

TEST_F(SignalCoreTest, Nextpow2Large)
{
    EXPECT_DOUBLE_EQ(evalScalar("nextpow2(1000)"), 10.0);
}

// ============================================================
// fft — basic properties
// ============================================================

TEST_F(SignalCoreTest, FftReturnsComplex)
{
    auto r = eval("fft([1 0 0 0])");
    EXPECT_TRUE(r.isComplex());
    EXPECT_EQ(r.numel(), 4u);
}

TEST_F(SignalCoreTest, FftDcComponent)
{
    // fft([1 1 1 1]) → DC = 4, rest = 0
    eval("X = fft([1 1 1 1]); A = abs(X);");
    EXPECT_NEAR(evalScalar("A(1)"), 4.0, 1e-10);
    EXPECT_NEAR(evalScalar("A(2)"), 0.0, 1e-10);
    EXPECT_NEAR(evalScalar("A(3)"), 0.0, 1e-10);
    EXPECT_NEAR(evalScalar("A(4)"), 0.0, 1e-10);
}

TEST_F(SignalCoreTest, FftImpulse)
{
    // fft([1 0 0 0]) → all magnitudes = 1
    eval("X = fft([1 0 0 0]); A = abs(X);");
    EXPECT_NEAR(evalScalar("A(1)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("A(2)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("A(3)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("A(4)"), 1.0, 1e-10);
}

TEST_F(SignalCoreTest, FftWithN)
{
    // fft(x, 8) zero-pads to 8
    auto r = eval("fft([1 2 3 4], 8)");
    EXPECT_EQ(r.numel(), 8u);
}

TEST_F(SignalCoreTest, FftWithNTruncates)
{
    // fft(x, 2) uses only first 2 elements
    auto r = eval("fft([1 2 3 4], 2)");
    EXPECT_EQ(r.numel(), 2u);
}

TEST_F(SignalCoreTest, FftParseval)
{
    // Energy in time domain = energy in frequency domain / N
    eval("x = [1 2 3 4 5 6 7 8];");
    eval("X = fft(x);");
    double timeEnergy = evalScalar("sum(x .* x)");
    double freqEnergy = evalScalar("sum(abs(X) .^ 2) / length(x)");
    EXPECT_NEAR(timeEnergy, freqEnergy, 1e-8);
}

// ============================================================
// ifft — inverse
// ============================================================

TEST_F(SignalCoreTest, IfftInverseOfFft)
{
    eval("x = [1 2 3 4 5 6 7 8];");
    eval("y = ifft(fft(x));");
    for (int i = 1; i <= 8; ++i) {
        std::string expr = "y(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(expr), static_cast<double>(i), 1e-10);
    }
}

TEST_F(SignalCoreTest, IfftReturnsRealForRealInput)
{
    auto r = eval("ifft(fft([1 2 3 4]))");
    // Should be real (imaginary part < 1e-10)
    EXPECT_TRUE(r.type() == MType::DOUBLE);
}

TEST_F(SignalCoreTest, IfftWithN)
{
    auto r = eval("ifft(fft([1 2 3 4]), 8)");
    EXPECT_EQ(r.numel(), 8u);
}

TEST_F(SignalCoreTest, FftEmptyN)
{
    // fft(x, []) — same as fft(x)
    auto r1 = eval("fft([1 2 3 4])");
    auto r2 = eval("fft([1 2 3 4], [])");
    EXPECT_EQ(r1.numel(), r2.numel());
    for (size_t i = 0; i < r1.numel(); ++i) {
        EXPECT_NEAR(r1.complexData()[i].real(), r2.complexData()[i].real(), 1e-10);
        EXPECT_NEAR(r1.complexData()[i].imag(), r2.complexData()[i].imag(), 1e-10);
    }
}

TEST_F(SignalCoreTest, FftAlongDim2)
{
    // fft along rows (dim=2): each row is transformed independently
    // [1 0 0 0; 1 1 1 1] — row 1 is impulse, row 2 is DC
    eval("X = fft([1 0 0 0; 1 1 1 1], [], 2);");
    // Row 1: fft([1 0 0 0]) → all magnitudes = 1
    EXPECT_NEAR(evalScalar("abs(X(1,1))"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("abs(X(1,2))"), 1.0, 1e-10);
    // Row 2: fft([1 1 1 1]) → DC=4, rest=0
    EXPECT_NEAR(evalScalar("abs(X(2,1))"), 4.0, 1e-10);
    EXPECT_NEAR(evalScalar("abs(X(2,2))"), 0.0, 1e-10);
}

TEST_F(SignalCoreTest, FftAlongDim1)
{
    // fft along columns (dim=1, default for matrix)
    eval("X = fft([1 1; 0 1; 0 1; 0 1], [], 1);");
    // Column 1: fft([1;0;0;0]) → all magnitudes = 1
    EXPECT_NEAR(evalScalar("abs(X(1,1))"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("abs(X(2,1))"), 1.0, 1e-10);
    // Column 2: fft([1;1;1;1]) → DC=4, rest=0
    EXPECT_NEAR(evalScalar("abs(X(1,2))"), 4.0, 1e-10);
    EXPECT_NEAR(evalScalar("abs(X(2,2))"), 0.0, 1e-10);
}

TEST_F(SignalCoreTest, FftWithNAndDim)
{
    // fft(x, 8, 2) — zero-pad rows to 8
    eval("X = fft([1 2 3 4], 8, 2);");
    EXPECT_EQ(eval("X").dims().cols(), 8u);
    EXPECT_EQ(eval("X").dims().rows(), 1u);
}

TEST_F(SignalCoreTest, FftWithNTruncateAndDim)
{
    // fft(x, 2, 2) — truncate rows to 2
    eval("X = fft([1 2 3 4; 5 6 7 8], 2, 2);");
    EXPECT_EQ(eval("X").dims().cols(), 2u);
    EXPECT_EQ(eval("X").dims().rows(), 2u);
}

TEST_F(SignalCoreTest, IfftAlongDim2)
{
    // Round-trip: ifft(fft(x,[],2),[],2) should recover x
    eval("x = [1 2 3 4; 5 6 7 8];");
    eval("y = ifft(fft(x, [], 2), [], 2);");
    EXPECT_NEAR(evalScalar("abs(y(1,1) - 1)"), 0.0, 1e-10);
    EXPECT_NEAR(evalScalar("abs(y(2,4) - 8)"), 0.0, 1e-10);
}

// ============================================================
// fftshift / ifftshift
// ============================================================

TEST_F(SignalCoreTest, FftshiftEvenLength)
{
    // [1 2 3 4] → [3 4 1 2]
    eval("y = fftshift([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 2.0);
}

TEST_F(SignalCoreTest, FftshiftOddLength)
{
    // [1 2 3 4 5] → [4 5 1 2 3] (shift by floor(5/2)=2)
    eval("y = fftshift([1 2 3 4 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 5.0);
}

TEST_F(SignalCoreTest, IfftshiftInverse)
{
    eval("x = [1 2 3 4 5 6 7 8];");
    eval("y = ifftshift(fftshift(x));");
    for (int i = 1; i <= 8; ++i) {
        std::string expr = "y(" + std::to_string(i) + ")";
        EXPECT_DOUBLE_EQ(evalScalar(expr), static_cast<double>(i));
    }
}

TEST_F(SignalCoreTest, FftshiftComplex)
{
    eval("X = fft([1 0 0 0]);");
    auto r = eval("fftshift(X)");
    EXPECT_TRUE(r.isComplex());
    EXPECT_EQ(r.numel(), 4u);
}

// ============================================================
// chirp
// ============================================================

TEST_F(SignalCoreTest, ChirpLinearAtT0EqualsCos0)
{
    // At t=0, regardless of f0/f1/t1, phase=0 ⇒ y[0] = cos(0) = 1.
    eval("t = 0:0.001:0.5;"
         "y = chirp(t, 0, 0.5, 50);");  // default 'linear'
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
}

TEST_F(SignalCoreTest, ChirpLinearShape)
{
    eval("t = linspace(0, 1, 100);"
         "y = chirp(t, 0, 1, 10);");
    auto y = eval("y");
    EXPECT_EQ(y.numel(), 100u);
    EXPECT_EQ(y.dims().rows(), 1u);
    EXPECT_EQ(y.dims().cols(), 100u);
}

TEST_F(SignalCoreTest, ChirpLinearMidpointPhase)
{
    // Linear chirp: phase(t) = 2π·(f0·t + 0.5·k·t²), k=(f1-f0)/t1.
    // f0=10, f1=30, t1=1, t=0.5: k=20, phase = 2π·(10·0.5 + 0.5·20·0.25)
    //   = 2π·(5 + 2.5) = 2π·7.5 = 15π ⇒ cos = cos(15π) = cos(π) = -1.
    eval("y = chirp(0.5, 10, 1, 30);");
    EXPECT_NEAR(evalScalar("y"), -1.0, 1e-12);
}

TEST_F(SignalCoreTest, ChirpQuadratic)
{
    // Quadratic: phase(t) = 2π·(f0·t + (k/3)·t³), k=(f1-f0)/t1²
    // f0=2, f1=8, t1=1 ⇒ k=6. At t=1: phase = 2π·(2 + 2) = 8π ⇒ cos=1.
    eval("y = chirp(1, 2, 1, 8, 'quadratic');");
    EXPECT_NEAR(evalScalar("y"), 1.0, 1e-12);
}

TEST_F(SignalCoreTest, ChirpLogarithmic)
{
    // Logarithmic: phase(t) = 2π·f0·((β^t - 1)/log(β)), β=(f1/f0)^(1/t1).
    // At t=0: phase=0 ⇒ y=1.
    eval("y = chirp(0, 1, 1, 100, 'logarithmic');");
    EXPECT_NEAR(evalScalar("y"), 1.0, 1e-12);
}

TEST_F(SignalCoreTest, ChirpLogarithmicNonNegativeFreqsRequired)
{
    // f0=0 not allowed for logarithmic.
    EXPECT_THROW(eval("y = chirp([0 0.5 1], 0, 1, 100, 'logarithmic');"),
                 std::exception);
}

TEST_F(SignalCoreTest, ChirpPreservesShape)
{
    // Column vector input → column vector output.
    eval("t = (0:0.1:0.5)';"
         "y = chirp(t, 0, 0.5, 10);");
    auto y = eval("y");
    EXPECT_EQ(y.dims().rows(), 6u);
    EXPECT_EQ(y.dims().cols(), 1u);
}

TEST_F(SignalCoreTest, ChirpInvalidMethodThrows)
{
    EXPECT_THROW(eval("y = chirp([0 0.5 1], 0, 1, 10, 'noSuchMethod');"),
                 std::exception);
}

TEST_F(SignalCoreTest, ChirpBadT1Throws)
{
    EXPECT_THROW(eval("y = chirp([0 0.5 1], 0, 0, 10);"), std::exception);
    EXPECT_THROW(eval("y = chirp([0 0.5 1], 0, -1, 10);"), std::exception);
}

// ============================================================
// rectpuls / tripuls / gauspuls / pulstran
// ============================================================

TEST_F(SignalCoreTest, RectpulsDefaultWidthOne)
{
    // Default width 1 → support is (-0.5, 0.5).
    eval("y = rectpuls([-0.6 -0.5 -0.3 0 0.3 0.5 0.6]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.0);  // boundary excluded
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(6)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(7)"), 0.0);
}

TEST_F(SignalCoreTest, RectpulsCustomWidth)
{
    eval("y = rectpuls([-1.5 -0.5 0 0.5 1.5], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 0.0);
}

TEST_F(SignalCoreTest, TripulsDefaultWidth)
{
    // y = max(1 - 2|t|/1, 0); at t=0 → 1, at t=±0.25 → 0.5, at t=±0.5 → 0.
    eval("y = tripuls([-0.5 -0.25 0 0.25 0.5]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 0.0);
}

TEST_F(SignalCoreTest, GauspulsAtZero)
{
    // gauspuls(0, fc) = exp(0) · cos(0) = 1.
    EXPECT_NEAR(evalScalar("gauspuls(0, 1);"), 1.0, 1e-12);
}

TEST_F(SignalCoreTest, GauspulsEnvelopeSymmetric)
{
    eval("y_pos = gauspuls(0.3, 1);"
         "y_neg = gauspuls(-0.3, 1);");
    EXPECT_NEAR(evalScalar("y_pos;"), evalScalar("y_neg;"), 1e-12);
}

TEST_F(SignalCoreTest, GauspulsBadArgsThrow)
{
    EXPECT_THROW(eval("y = gauspuls([0 0.1], -1);"), std::exception);
    EXPECT_THROW(eval("y = gauspuls([0 0.1], 1, -0.5);"), std::exception);
}

TEST_F(SignalCoreTest, PulstranRectSumsTwoPulses)
{
    // Two delays: 0 and 5, width 1 → unit pulse at each.
    eval("t = -0.6:0.2:5.6;"  // includes both supports
         "y = pulstran(t, [0, 5], 'rectpuls', 1);");
    // Pulse at delay 0 covers t ∈ (-0.5, 0.5); pulse at delay 5 covers t ∈ (4.5, 5.5).
    EXPECT_DOUBLE_EQ(evalScalar("y(1);"), 0.0);   // t = -0.6
    EXPECT_DOUBLE_EQ(evalScalar("y(3);"), 1.0);   // t = -0.2 (in pulse 0)
    EXPECT_DOUBLE_EQ(evalScalar("y(28);"), 1.0);  // t ≈ 4.8 (in pulse 1)
    // Sum at points where both pulses overlap = 2; verify they don't here.
    EXPECT_LE(evalScalar("max(y);"), 1.0);
}

TEST_F(SignalCoreTest, PulstranTriDecayingHeights)
{
    // tripuls(0, 1) = 1; sum at t=0 of pulses centred at 0 and 0.5 (overlap):
    eval("y = pulstran(0, [0, 0.5], 'tripuls', 1);");
    // pulse @ 0 evaluated at 0 → 1; pulse @ 0.5 evaluated at -0.5 → tripuls(-0.5, 1) = 0.
    EXPECT_DOUBLE_EQ(evalScalar("y;"), 1.0);
}

TEST_F(SignalCoreTest, PulstranGausPulsLinear)
{
    // Sanity: pulstran(t, [0], 'gauspuls', fc) should match gauspuls(t, fc) exactly.
    eval("t = (-2:0.1:2);"
         "y_train = pulstran(t, [0], 'gauspuls', 1);"
         "y_dir   = gauspuls(t, 1);"
         "delta = max(abs(y_train - y_dir));");
    EXPECT_LT(evalScalar("delta;"), 1e-12);
}

TEST_F(SignalCoreTest, PulstranUnknownFnThrows)
{
    EXPECT_THROW(eval("y = pulstran([0 1 2], [0 1], 'noSuchPulse');"), std::exception);
}

TEST_F(SignalCoreTest, PulstranAnonHandleThrows)
{
    // Custom function handles aren't supported until the engine callback API.
    EXPECT_THROW(eval("y = pulstran([0 1 2], [0 1], @(t) cos(t));"), std::exception);
}
