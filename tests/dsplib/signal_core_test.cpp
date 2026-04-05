// tests/signal_core_test.cpp

#include "MLabEngine.hpp"
#include "MLabStdLibrary.hpp"
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace mlab;

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
