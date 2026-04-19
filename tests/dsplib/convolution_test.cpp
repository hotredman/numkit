// tests/convolution_test.cpp

#include "MEngine.hpp"
#include "MStdLibrary.hpp"
#include <cmath>
#include <gtest/gtest.h>

using namespace mlab;

class ConvolutionTest : public ::testing::Test
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
// conv — full (default)
// ============================================================

TEST_F(ConvolutionTest, ConvFullLength)
{
    // conv([1 2 3], [4 5]) → length 3+2-1 = 4
    auto r = eval("conv([1 2 3], [4 5])");
    EXPECT_EQ(r.numel(), 4u);
}

TEST_F(ConvolutionTest, ConvFullValues)
{
    // [1 2 3] * [4 5] = [4 13 22 15]
    eval("c = conv([1 2 3], [4 5]);");
    EXPECT_NEAR(evalScalar("c(1)"), 4.0, 1e-10);
    EXPECT_NEAR(evalScalar("c(2)"), 13.0, 1e-10);
    EXPECT_NEAR(evalScalar("c(3)"), 22.0, 1e-10);
    EXPECT_NEAR(evalScalar("c(4)"), 15.0, 1e-10);
}

TEST_F(ConvolutionTest, ConvIdentity)
{
    // conv(x, [1]) = x
    eval("c = conv([1 2 3 4], [1]);");
    EXPECT_NEAR(evalScalar("c(1)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("c(4)"), 4.0, 1e-10);
    EXPECT_EQ(eval("c").numel(), 4u);
}

TEST_F(ConvolutionTest, ConvCommutative)
{
    eval("a = conv([1 2 3], [4 5 6]);");
    eval("b = conv([4 5 6], [1 2 3]);");
    for (int i = 1; i <= 5; ++i) {
        std::string ai = "a(" + std::to_string(i) + ")";
        std::string bi = "b(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(ai), evalScalar(bi), 1e-10);
    }
}

// ============================================================
// conv — same, valid
// ============================================================

TEST_F(ConvolutionTest, ConvSameLength)
{
    // 'same' returns max(na, nb)
    auto r = eval("conv([1 2 3 4 5], [1 1 1], 'same')");
    EXPECT_EQ(r.numel(), 5u);
}

TEST_F(ConvolutionTest, ConvValidLength)
{
    // 'valid' returns max(na,nb)-min(na,nb)+1 = 5-3+1 = 3
    auto r = eval("conv([1 2 3 4 5], [1 1 1], 'valid')");
    EXPECT_EQ(r.numel(), 3u);
}

TEST_F(ConvolutionTest, ConvValidValues)
{
    // Moving average: conv([1 2 3 4 5], [1 1 1], 'valid') = [6 9 12]
    eval("c = conv([1 2 3 4 5], [1 1 1], 'valid');");
    EXPECT_NEAR(evalScalar("c(1)"), 6.0, 1e-10);
    EXPECT_NEAR(evalScalar("c(2)"), 9.0, 1e-10);
    EXPECT_NEAR(evalScalar("c(3)"), 12.0, 1e-10);
}

// ============================================================
// conv — FFT path (large inputs)
// ============================================================

TEST_F(ConvolutionTest, ConvLargeMatchesDirect)
{
    // Both paths should give same result
    eval("a = ones(1, 600); b = ones(1, 600);");
    eval("c = conv(a, b);");
    // Convolution of two rectangular pulses → triangle
    // Peak at center = 600
    EXPECT_NEAR(evalScalar("max(c)"), 600.0, 1e-6);
    EXPECT_EQ(eval("c").numel(), 1199u);
}

// ============================================================
// deconv
// ============================================================

TEST_F(ConvolutionTest, DeconvRecovery)
{
    // If b = conv(a, q), then deconv(b, a) = q
    eval("a = [1 2 1]; q = [3 4 5];");
    eval("b = conv(a, q);");
    eval("[qr, r] = deconv(b, a);");
    EXPECT_NEAR(evalScalar("qr(1)"), 3.0, 1e-10);
    EXPECT_NEAR(evalScalar("qr(2)"), 4.0, 1e-10);
    EXPECT_NEAR(evalScalar("qr(3)"), 5.0, 1e-10);
}

TEST_F(ConvolutionTest, DeconvRemainder)
{
    // Polynomial division: (x^3 + 2x^2 + 3x + 4) / (x + 1)
    eval("[q, r] = deconv([1 2 3 4], [1 1]);");
    // q = [1 1 2], remainder should have leading zeros
    EXPECT_NEAR(evalScalar("q(1)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("q(2)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("q(3)"), 2.0, 1e-10);
    // r(4) = 4 - 2*1 = 2
    EXPECT_NEAR(evalScalar("r(4)"), 2.0, 1e-10);
}

// ============================================================
// xcorr
// ============================================================

TEST_F(ConvolutionTest, XcorrAutoLength)
{
    // xcorr([1 2 3]) → length 2*3-1 = 5
    eval("[c, lags] = xcorr([1 2 3]);");
    EXPECT_EQ(eval("c").numel(), 5u);
    EXPECT_EQ(eval("lags").numel(), 5u);
}

TEST_F(ConvolutionTest, XcorrAutoPeak)
{
    // Auto-correlation peak at lag 0 = sum(x.^2)
    eval("[c, lags] = xcorr([1 2 3]);");
    double peak = evalScalar("max(c)");
    double energy = evalScalar("sum([1 2 3] .^ 2)"); // 14
    EXPECT_NEAR(peak, energy, 1e-10);
}

TEST_F(ConvolutionTest, XcorrAutoSymmetric)
{
    // Auto-correlation is symmetric
    eval("[c, lags] = xcorr([1 2 3 4]);");
    size_t n = eval("c").numel();
    for (size_t i = 0; i < n / 2; ++i) {
        std::string left = "c(" + std::to_string(i + 1) + ")";
        std::string right = "c(" + std::to_string(n - i) + ")";
        EXPECT_NEAR(evalScalar(left), evalScalar(right), 1e-10);
    }
}

TEST_F(ConvolutionTest, XcorrCrossLength)
{
    eval("[c, lags] = xcorr([1 2 3], [4 5]);");
    // length = 3 + 2 - 1 = 4
    EXPECT_EQ(eval("c").numel(), 4u);
}

TEST_F(ConvolutionTest, XcorrLagsRange)
{
    eval("[c, lags] = xcorr([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("lags(1)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("lags(4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("lags(7)"), 3.0);
}
