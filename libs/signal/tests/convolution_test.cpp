// tests/convolution_test.cpp

#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace numkit;

class ConvolutionTest : public ::testing::Test
{
public:
    Engine engine;
    std::string capturedOutput;

    void SetUp() override
    {
        capturedOutput.clear();
        engine.setOutputFunc([this](const std::string &s) { capturedOutput += s; });
        engine.eval("import compat.*;");
    }

    Value eval(const std::string &code) { return engine.eval(code); }
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
    // 'same' returns the central part the SAME SIZE AS THE FIRST input (na).
    auto r = eval("conv([1 2 3 4 5], [1 1 1], 'same')");
    EXPECT_EQ(r.numel(), 5u);
}

// 'same' = central part of length na, starting at floor(nb/2) of the full
// convolution. Previously numkit used max(na,nb) + a centered offset, which
// was off-by-one for EVEN kernels and wrong-length when na<nb. vs MATLAB
// R2025b. DEEP-PROBE 2026-05-30.
TEST_F(ConvolutionTest, ConvSameEvenKernelAndShortFirst)
{
    // Even kernel (nb=2): conv([1 2 3 4],[1 1],'same') = [3 5 7 4] (not [1 3 5 7]).
    eval("e = conv([1 2 3 4], [1 1], 'same');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(e)"), 4.0);
    EXPECT_NEAR(evalScalar("e(1)"), 3.0, 1e-10);
    EXPECT_NEAR(evalScalar("e(4)"), 4.0, 1e-10);
    // Odd first arg, even kernel: conv([1 2 3],[1 1],'same') = [3 5 3].
    eval("f = conv([1 2 3], [1 1], 'same');");
    EXPECT_NEAR(evalScalar("f(1)"), 3.0, 1e-10);
    EXPECT_NEAR(evalScalar("f(3)"), 3.0, 1e-10);
    // nb=4: conv([1 2 3 4 5],[1 1 1 1],'same') = [6 10 14 12 9].
    eval("g = conv([1 2 3 4 5], [1 1 1 1], 'same');");
    EXPECT_NEAR(evalScalar("g(1)"), 6.0, 1e-10);
    EXPECT_NEAR(evalScalar("g(5)"), 9.0, 1e-10);
    // First arg SHORTER than kernel -> length na (not nb):
    // conv([1 2],[1 1 1 1 1],'same') = [3 3] (length 2).
    eval("h = conv([1 2], [1 1 1 1 1], 'same');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(h)"), 2.0);
    EXPECT_NEAR(evalScalar("h(1)"), 3.0, 1e-10);
    EXPECT_NEAR(evalScalar("h(2)"), 3.0, 1e-10);
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

// The shape arg accepts a STRING ("same") as well as a char ('same') —
// MATLAB R2025b accepts both. conv_reg previously checked only isChar(), so
// a double-quoted shape was SILENTLY IGNORED and the result fell back to the
// full convolution: conv([1 2 3 4],[1 1],"same") gave [1 3 5 7 4] (length 5)
// instead of [3 5 7 4] (length 4). DEEP-PROBE 2026-05-31.
TEST_F(ConvolutionTest, ConvShapeAcceptsString)
{
    // "same" (string) must match 'same' (char) = [3 5 7 4], length 4.
    eval("e = conv([1 2 3 4], [1 1], \"same\");");
    EXPECT_DOUBLE_EQ(evalScalar("numel(e)"), 4.0);
    EXPECT_NEAR(evalScalar("e(1)"), 3.0, 1e-10);
    EXPECT_NEAR(evalScalar("e(2)"), 5.0, 1e-10);
    EXPECT_NEAR(evalScalar("e(3)"), 7.0, 1e-10);
    EXPECT_NEAR(evalScalar("e(4)"), 4.0, 1e-10);
    // "valid" (string) = [6 9 12], length 3.
    eval("v = conv([1 2 3 4 5], [1 1 1], \"valid\");");
    EXPECT_DOUBLE_EQ(evalScalar("numel(v)"), 3.0);
    EXPECT_NEAR(evalScalar("v(1)"), 6.0, 1e-10);
    EXPECT_NEAR(evalScalar("v(3)"), 12.0, 1e-10);
    // "full" (string) explicit = same as default, length 5.
    eval("f = conv([1 2 3 4], [1 1], \"full\");");
    EXPECT_DOUBLE_EQ(evalScalar("numel(f)"), 5.0);
    EXPECT_NEAR(evalScalar("f(1)"), 1.0, 1e-10);
    EXPECT_NEAR(evalScalar("f(5)"), 4.0, 1e-10);
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

// xcorr scaleopt (was accepted-and-ignored -> raw correlation). vs MATLAB.
// xcorr([1 2 3]) raw = [3 8 14 8 3]; energy at lag 0 = 14.
TEST_F(ConvolutionTest, XcorrScaleOpts)
{
    eval("c = xcorr([1 2 3], [1 2 3], 'coeff');");   // peak normalized to 1
    EXPECT_NEAR(evalScalar("c(3)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("c(2)"), 8.0 / 14.0, 1e-12);
    eval("cb = xcorr([1 2 3], [1 2 3], 'biased');"); // divide by N=3
    EXPECT_NEAR(evalScalar("cb(3)"), 14.0 / 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("cb(1)"), 1.0, 1e-12);
    eval("cu = xcorr([1 2 3], [1 2 3], 'unbiased');"); // divide by N-|lag|
    EXPECT_NEAR(evalScalar("cu(3)"), 14.0 / 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("cu(2)"), 8.0 / 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("cu(1)"), 3.0 / 1.0, 1e-12);
    // single-arg autocorr honors scaleopt too
    eval("ca = xcorr([1 2 3], 'coeff');");
    EXPECT_NEAR(evalScalar("ca(3)"), 1.0, 1e-12);
    // unknown scaleopt throws
    EXPECT_THROW(eval("xcorr([1 2 3], [1 2 3], 'bogus');"), std::exception);
}

// xcorr maxlag crop (+ combined with scaleopt).
TEST_F(ConvolutionTest, XcorrMaxLag)
{
    eval("[c, lags] = xcorr([1 2 3], [1 2 3], 1);");
    EXPECT_EQ(eval("c").numel(), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("lags(1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("lags(3)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 14.0);      // lag 0
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"),  8.0);      // lag -1
    eval("c2 = xcorr([1 2 3], [1 2 3], 1, 'coeff');");
    EXPECT_NEAR(evalScalar("c2(2)"), 1.0, 1e-12);
}

// ── Pack 36: conv2 / filter2 / convn ────────────────────────────────
TEST_F(ConvolutionTest, Conv2FullKnownExample)
{
    // A=[1 2 3;4 5 6;7 8 9], B=[1 0;0 -1] → MATLAB-verified result.
    eval("A = [1 2 3; 4 5 6; 7 8 9]; B = [1 0; 0 -1];");
    eval("C = conv2(A, B);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(1,4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(4,4)"), -9.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(C,2)"), 4.0);
}

TEST_F(ConvolutionTest, Conv2SameMatchesMatlab)
{
    // 'same' for P=Q=2 should drop 1 row from top, 1 col from left.
    eval("A = [1 2 3; 4 5 6; 7 8 9]; B = [1 0; 0 -1];");
    eval("S = conv2(A, B, 'same');");
    EXPECT_DOUBLE_EQ(evalScalar("S(1,1)"),  4.0);
    EXPECT_DOUBLE_EQ(evalScalar("S(1,3)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("S(3,3)"), -9.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(S,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(S,2)")), 3);
}

TEST_F(ConvolutionTest, Conv2ValidShape)
{
    eval("A = [1 2 3; 4 5 6; 7 8 9]; B = [1 0; 0 -1];");
    eval("V = conv2(A, B, 'valid');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(V,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(V,2)")), 2);
    // conv2 flips B: out(i,j) = sum A(i+p, j+q) * B(P-1-p, Q-1-q).
    // For B=[1 0; 0 -1], flipped is [-1 0; 0 1] → out = A(i+1,j+1) - A(i,j).
    EXPECT_DOUBLE_EQ(evalScalar("V(1,1)"), 5.0 - 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("V(2,2)"), 9.0 - 5.0);
}

TEST_F(ConvolutionTest, Filter2EquivalentToConv2WithRot90)
{
    eval("A = [1 2 3; 4 5 6; 7 8 9]; h = [1 0; 0 -1];");
    eval("F = filter2(h, A);");
    // filter2 default 'same' = conv2(A, rot90(h, 2), 'same')
    // rot90(h,2) = [-1 0; 0 1]; expected from MATLAB probe.
    EXPECT_DOUBLE_EQ(evalScalar("F(1,1)"), -4.0);
    EXPECT_DOUBLE_EQ(evalScalar("F(1,3)"),  3.0);
    EXPECT_DOUBLE_EQ(evalScalar("F(3,3)"),  9.0);
}

TEST_F(ConvolutionTest, ConvnFallsThroughToConv2For2D)
{
    eval("A = [1 2; 3 4]; B = [1 -1];");
    eval("Cn = convn(A, B);");
    eval("C2 = conv2(A, B);");
    eval("delta = max(abs(Cn(:) - C2(:)));");
    EXPECT_LT(evalScalar("delta"), 1e-12);
}

TEST_F(ConvolutionTest, Conv2SeparableImpulseIdentity)
{
    // Convolving by [1] should be identity in 'same' mode.
    eval("A = [2 7 6; 9 5 1; 4 3 8];");
    eval("F = conv2(A, [1], 'same');");
    eval("delta = max(abs(F(:) - A(:)));");
    EXPECT_LT(evalScalar("delta"), 1e-12);
}

// ── Pack 36: xcov ────────────────────────────────────────────────────
TEST_F(ConvolutionTest, XcovOnConstantIsZero)
{
    // For a constant signal, xcov returns ~0 everywhere.
    eval("c = xcov([5 5 5 5]);");
    EXPECT_LT(evalScalar("max(abs(c(:)))"), 1e-12);
}

TEST_F(ConvolutionTest, XcovEqualsXcorrOnCenteredSignal)
{
    eval("x = [1 2 3 4 5];"
         "xc = x - mean(x);"
         "[a, ~] = xcorr(xc);"
         "[b, ~] = xcov(x);"
         "delta = max(abs(a(:) - b(:)));");
    EXPECT_LT(evalScalar("delta"), 1e-12);
}

TEST_F(ConvolutionTest, XcovTwoSignalsLength)
{
    eval("[c, lags] = xcov([1 2 3 4], [4 3 2 1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(c)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("length(lags)")), 7);
}

// scaleopt: 'biased' divides every lag by N; 'unbiased' by (N-|lag|);
// 'coeff' by sqrt(Cxx0*Cyy0). zero-lag sits at index 5 for length-5 inputs.
// (Regression: numkit used to ignore scaleopt entirely.) vs MATLAB R2025b.
TEST_F(ConvolutionTest, XcovScaleopt)
{
    eval("x = [1 3 -2 4 0]; y = [2 -1 0 3 1];");
    eval("cn = xcov(x,y); cb = xcov(x,y,'biased');");
    EXPECT_NEAR(evalScalar("cn(5)"), 5.0, 1e-12);     // raw zero-lag
    EXPECT_NEAR(evalScalar("cb(5)"), 1.0, 1e-12);     // /N=5
    EXPECT_NEAR(evalScalar("cb(4)"), -1.56, 1e-12);
    eval("cu = xcov(x,y,'unbiased'); cc = xcov(x,y,'coeff');");
    EXPECT_NEAR(evalScalar("cu(3)"), 1.2666666666666666, 1e-12);
    EXPECT_NEAR(evalScalar("cc(5)"), 0.331133089266, 1e-9);
}

// maxlag crops the result to lags -maxlag..maxlag (length 2*maxlag+1).
// (Regression: numkit used to ignore the maxlag scalar arg, returning
// the full length-9 vector.) vs MATLAB R2025b.
TEST_F(ConvolutionTest, XcovMaxlag)
{
    eval("x = [1 3 -2 4 0]; y = [2 -1 0 3 1];");
    eval("cm = xcov(x,y,2);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(cm)")), 5);
    EXPECT_NEAR(evalScalar("cm(3)"), 5.0, 1e-12);     // zero-lag at center
    EXPECT_NEAR(evalScalar("cm(1)"), 3.8, 1e-12);
    EXPECT_NEAR(evalScalar("cm(5)"), -7.6, 1e-12);
    eval("cmb = xcov(x,y,2,'biased');");              // maxlag + scaleopt
    EXPECT_NEAR(evalScalar("cmb(3)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("cmb(1)"), 0.76, 1e-12);
}
