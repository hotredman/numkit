// toolboxes/image/tests/adapthisteq_test.cpp
// gtest coverage for adapthisteq — Contrast-Limited Adaptive Histogram
// Equalisation (CLAHE).
// adapthisteq is a clean-room implementation from public references
// (Zuiderveld 1994; Pizer et al. 1990 / 1987 — see
//). It is functionally equivalent to
// MATLAB's adapthisteq but not bit-identical: MATLAB's clip/redistribute
// and interpolation rounding have undocumented details. Accordingly the
// pixel-value tests below are *regression anchors* pinned to the
// clean-room output, NOT bit-exact MATLAB parity claims. Real
// correctness is verified MATLAB-independently by the property tests at
// the end of this file (a correct CLAHE must widen a low-contrast
// image's dynamic range, and more clipping must spread it further).

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class AdaptHistEqTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void   SetUp() override { engine.eval("import compat.*;"); }
    double eval_scalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// Output preserves input class + size.
TEST_F(AdaptHistEqTest, PreservesClassAndSize)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    engine.eval("J = adapthisteq(I);");
    EXPECT_EQ(eval_scalar("strcmp(class(J), 'uint8')"), 1.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(J, 1)"), 16.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(J, 2)"), 16.0);
}

// Regression anchor — smooth gradient, default options. Values pinned to
// the clean-room CLAHE output (not MATLAB). Guards against accidental
// algorithm drift.
TEST_F(AdaptHistEqTest, GradientRegressionAnchor)
{
    engine.eval("[X, Y] = meshgrid(linspace(0, 1, 64), linspace(0, 1, 64));");
    engine.eval("I = uint8(255 * sqrt(X.*Y));");
    engine.eval("J = adapthisteq(I);");
    EXPECT_DOUBLE_EQ(eval_scalar("double(J(1, 1))"),     8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("double(J(16, 16))"), 219.0);
    EXPECT_DOUBLE_EQ(eval_scalar("double(J(32, 32))"), 217.0);
    EXPECT_DOUBLE_EQ(eval_scalar("double(J(48, 48))"), 221.0);
    EXPECT_DOUBLE_EQ(eval_scalar("double(J(64, 64))"), 255.0);
}

// Regression anchor — 8x8 rotational pattern, [2 2] tiles. Values
// pinned to the clean-room CLAHE output.
TEST_F(AdaptHistEqTest, SmallPatternRegressionAnchor)
{
    engine.eval("I = uint8([0 32 64 96 128 160 192 224;"
                          " 32 64 96 128 160 192 224 32;"
                          " 64 96 128 160 192 224 32 64;"
                          " 96 128 160 192 224 32 64 96;"
                          " 128 160 192 224 32 64 96 128;"
                          " 160 192 224 32 64 96 128 160;"
                          " 192 224 32 64 96 128 160 192;"
                          " 224 32 64 96 128 160 192 224]);");
    engine.eval("J = adapthisteq(I, 'NumTiles', [2 2], 'ClipLimit', 0.01);");
    EXPECT_DOUBLE_EQ(eval_scalar("double(J(1, 1))"),  19.0);
    EXPECT_DOUBLE_EQ(eval_scalar("double(J(1, 4))"), 252.0);
    EXPECT_DOUBLE_EQ(eval_scalar("double(J(4, 5))"), 255.0);
    EXPECT_DOUBLE_EQ(eval_scalar("double(J(8, 8))"), 255.0);
}

// NumTiles must be >= 2.
TEST_F(AdaptHistEqTest, BadNumTilesThrows)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    EXPECT_THROW(engine.eval("adapthisteq(I, 'NumTiles', [1 1]);"), std::exception);
}

// ClipLimit out of [0, 1] throws.
TEST_F(AdaptHistEqTest, BadClipLimitThrows)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    EXPECT_THROW(engine.eval("adapthisteq(I, 'ClipLimit', 1.5);"), std::exception);
}

// rayleigh / exponential distributions run and yield a valid image of
// the input class (full MATLAB argument set — these are supported, not
// deferred).
TEST_F(AdaptHistEqTest, NonUniformDistributionsRun)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");

    engine.eval("R = adapthisteq(I, 'Distribution', 'rayleigh');");
    EXPECT_EQ(eval_scalar("strcmp(class(R), 'uint8')"), 1.0);
    EXPECT_EQ(eval_scalar("isequal(size(R), [16 16])"), 1.0);

    engine.eval("E = adapthisteq(I, 'Distribution', 'exponential');");
    EXPECT_EQ(eval_scalar("strcmp(class(E), 'uint8')"), 1.0);
    EXPECT_EQ(eval_scalar("isequal(size(E), [16 16])"), 1.0);

    // Alpha is accepted alongside a non-uniform distribution.
    engine.eval("RA = adapthisteq(I, 'Distribution', 'rayleigh', 'Alpha', 0.8);");
    EXPECT_EQ(eval_scalar("isequal(size(RA), [16 16])"), 1.0);
}

// An unrecognised Distribution string throws.
TEST_F(AdaptHistEqTest, BadDistributionThrows)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    EXPECT_THROW(engine.eval("adapthisteq(I, 'Distribution', 'bogus');"),
                 std::exception);
}

// An unrecognised Range string throws.
TEST_F(AdaptHistEqTest, BadRangeThrows)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    EXPECT_THROW(engine.eval("adapthisteq(I, 'Range', 'bogus');"),
                 std::exception);
}

// Unknown name-value key throws.
TEST_F(AdaptHistEqTest, UnknownNVKeyThrows)
{
    engine.eval("I = uint8(reshape(0:255, 16, 16));");
    EXPECT_THROW(engine.eval("adapthisteq(I, 'BogusKey', 5);"), std::exception);
}

// Double-precision input also works and stays in [0, 1].
TEST_F(AdaptHistEqTest, DoubleInputStaysInUnitRange)
{
    engine.eval("[X, Y] = meshgrid(linspace(0, 1, 64), linspace(0, 1, 64));");
    engine.eval("I = sqrt(X.*Y);");
    engine.eval("J = adapthisteq(I);");
    EXPECT_GE(eval_scalar("min(J(:))"), 0.0);
    EXPECT_LE(eval_scalar("max(J(:))"), 1.0);
    EXPECT_EQ(eval_scalar("strcmp(class(J), 'double')"), 1.0);
}

// ── MATLAB-independent correctness tests ──────────────────────────────
// These verify the *defining property* of CLAHE against a known answer
// — no reference engine involved. A correct implementation must spread
// a low-contrast image across a much wider intensity range.

// A low-contrast image (all pixels squeezed into a narrow sub-band)
// must come out with a substantially wider dynamic range and standard
// deviation. This is the core purpose of histogram equalisation.
TEST_F(AdaptHistEqTest, LowContrastInputGainsDynamicRange)
{
    // Pixels confined to [120, 136] — a 16-level band out of 256.
    engine.eval("[X, Y] = meshgrid(linspace(0, 1, 64), linspace(0, 1, 64));");
    engine.eval("lo = uint8(120 + 16 * sqrt(X.*Y));");
    engine.eval("hi = adapthisteq(lo, 'ClipLimit', 1.0);");

    const double inRange  = eval_scalar("double(max(lo(:))) - double(min(lo(:)))");
    const double outRange = eval_scalar("double(max(hi(:))) - double(min(hi(:)))");
    const double inStd    = eval_scalar("std(double(lo(:)))");
    const double outStd   = eval_scalar("std(double(hi(:)))");

    // Input is genuinely low-contrast.
    EXPECT_LE(inRange, 20.0);
    // Output spans a far wider range and is far more spread out.
    EXPECT_GT(outRange, 100.0);
    EXPECT_GT(outStd, 5.0 * inStd);
}

// More clipping headroom must spread the histogram further: a higher
// ClipLimit yields a larger output standard deviation than ClipLimit=0
// on a low-contrast input.
TEST_F(AdaptHistEqTest, ClipLimitOrderingIncreasesSpread)
{
    engine.eval("[X, Y] = meshgrid(linspace(0, 1, 64), linspace(0, 1, 64));");
    engine.eval("lo = uint8(120 + 16 * sqrt(X.*Y));");
    engine.eval("c0 = adapthisteq(lo, 'ClipLimit', 0.0);");
    engine.eval("c1 = adapthisteq(lo, 'ClipLimit', 1.0);");

    const double std0 = eval_scalar("std(double(c0(:)))");
    const double std1 = eval_scalar("std(double(c1(:)))");
    EXPECT_GT(std1, std0);
}

// Range='original' confines the output to the input's actual [min, max]
// instead of the full class range.
TEST_F(AdaptHistEqTest, RangeOriginalConstrainsOutput)
{
    engine.eval("[X, Y] = meshgrid(linspace(0, 1, 64), linspace(0, 1, 64));");
    engine.eval("lo = uint8(120 + 16 * sqrt(X.*Y));");
    engine.eval("ro = adapthisteq(lo, 'Range', 'original');");

    const double inMin  = eval_scalar("double(min(lo(:)))");
    const double inMax  = eval_scalar("double(max(lo(:)))");
    const double outMin = eval_scalar("double(min(ro(:)))");
    const double outMax = eval_scalar("double(max(ro(:)))");

    EXPECT_GE(outMin, inMin);
    EXPECT_LE(outMax, inMax);
}
