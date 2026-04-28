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
