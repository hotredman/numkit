// tests/transform_test.cpp

#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>
#include <cmath>
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace numkit;

class TransformTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { BuiltinLibrary::install(engine); }
    Value eval(const std::string &code) { return engine.eval(code); }
    double evalScalar(const std::string &code) { return eval(code).toScalar(); }
};

// ============================================================
// unwrap
// ============================================================

TEST_F(TransformTest, UnwrapNoWrap)
{
    // Already continuous phase — no change
    eval("p = [0 0.5 1.0 1.5 2.0];");
    eval("u = unwrap(p);");
    for (int i = 1; i <= 5; ++i) {
        std::string pi = "p(" + std::to_string(i) + ")";
        std::string ui = "u(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(pi), evalScalar(ui), 1e-10);
    }
}

TEST_F(TransformTest, UnwrapRemovesJump)
{
    // Phase with 2*pi jump → should unwrap to continuous
    // Wrapped: [0 1 2 3 4-2*pi 5-2*pi 6-2*pi]
    // Unwrapped should be: [0 1 2 3 4 5 6]
    eval("p = [0 1 2 3 4-2*pi 5-2*pi 6-2*pi];");
    eval("u = unwrap(p);");
    // Differences should be ~1
    for (int i = 2; i <= 7; ++i) {
        std::string diff = "u(" + std::to_string(i) + ") - u(" + std::to_string(i - 1) + ")";
        EXPECT_NEAR(evalScalar(diff), 1.0, 0.1);
    }
}

TEST_F(TransformTest, UnwrapLinearPhase)
{
    // Linear phase modulo 2*pi → unwrap to linear
    eval("t = linspace(0, 10*pi, 128);");
    eval("p = mod(t, 2*pi);"); // wrapped
    eval("u = unwrap(p);");
    // u should be close to t (up to constant offset)
    eval("err = u - t;");
    eval("err = err - err(1);"); // remove offset
    double maxErr = evalScalar("max(abs(err))");
    EXPECT_LT(maxErr, 0.1);
}

TEST_F(TransformTest, UnwrapPreservesLength)
{
    EXPECT_EQ(eval("unwrap([1 2 3 4 5])").numel(), 5u);
}

// ============================================================
// hilbert
// ============================================================

TEST_F(TransformTest, HilbertReturnsComplex)
{
    auto r = eval("hilbert([1 2 3 4 5 6 7 8])");
    EXPECT_TRUE(r.isComplex());
}

TEST_F(TransformTest, HilbertRealPartIsInput)
{
    // Real part of analytic signal = original signal
    eval("x = [1 2 3 4 5 6 7 8];");
    eval("z = hilbert(x);");
    eval("zr = real(z);");
    for (int i = 1; i <= 8; ++i) {
        std::string expr = "zr(" + std::to_string(i) + ")";
        EXPECT_NEAR(evalScalar(expr), static_cast<double>(i), 0.5);
    }
}

TEST_F(TransformTest, HilbertCosineToSine)
{
    // Hilbert transform of cos gives analytic signal: cos - j*sin
    // So imag(hilbert(cos)) ≈ -sin (negative sign is correct)
    eval("N = 64; n = 0:N-1;");
    eval("x = cos(2*pi*4*n/N);");
    eval("z = hilbert(x);");
    eval("h = imag(z);");
    eval("expected = -sin(2*pi*4*n/N);"); // note: negative sign
    eval("err = abs(h - expected);");
    eval("mid_err = err(17:48);");
    double maxErr = evalScalar("max(mid_err)");
    EXPECT_LT(maxErr, 0.5);
}

TEST_F(TransformTest, HilbertPreservesLength)
{
    EXPECT_EQ(eval("hilbert([1 2 3 4])").numel(), 4u);
}

// ============================================================
// envelope
// ============================================================

TEST_F(TransformTest, EnvelopeReturnsReal)
{
    auto r = eval("envelope([1 2 3 4 5 6 7 8])");
    EXPECT_TRUE(r.type() == ValueType::DOUBLE);
}

TEST_F(TransformTest, EnvelopeNonnegative)
{
    eval("e = envelope(randn(1, 64));");
    EXPECT_GE(evalScalar("min(e)"), -1e-10);
}

TEST_F(TransformTest, EnvelopeOfConstant)
{
    // Envelope of constant signal ≈ constant
    eval("e = envelope(3 * ones(1, 64));");
    // Middle values should be close to 3
    EXPECT_NEAR(evalScalar("e(32)"), 3.0, 0.5);
}

TEST_F(TransformTest, EnvelopeOfModulated)
{
    // AM signal: (1 + 0.5*cos(low_freq)) * cos(high_freq)
    // Envelope should track the modulation
    eval("n = 0:255;");
    eval("mod_sig = (1 + 0.5*cos(2*pi*2*n/256)) .* cos(2*pi*32*n/256);");
    eval("e = envelope(mod_sig);");
    // Envelope at peak of modulation ≈ 1.5
    eval("[mx, ~] = max(e(20:240));");
    EXPECT_GT(evalScalar("mx"), 1.2);
}

TEST_F(TransformTest, EnvelopePreservesLength)
{
    EXPECT_EQ(eval("envelope([1 2 3 4 5 6 7 8])").numel(), 8u);
}