// libs/image/tests/imreducehaze_test.cpp
//
// Regression guard for imreducehaze — single-image dehazing via
// dark-channel prior. The simpledcp (default) path is BIT-EXACT
// with MATLAB R2025b on probed inputs because we reuse the same
// helpers (imerode, rgb2gray, mat2gray, stretchlim, imadjust,
// imguidedfilter). The approxdcp path diverges by ~15-30 uint8
// because (a) numkit's imhist uses slightly different bucket
// boundaries than MATLAB and (b) numkit's imguidedfilter doesn't
// expose the Fast-Guided-Filter subsample optimization.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImReduceHazeTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval(
            "import compat.*;"
            "rng(0);"
            "A = uint8(255 * rand(32, 32, 3));"
            "A = uint8(min(255, double(A) + 80));");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Default path (simpledcp + global stretch) — bit-exact ───────

TEST_F(ImReduceHazeTest, DefaultClassAndSize)
{
    eval("[B, T, L] = imreducehaze(A);");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 32);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 2)")), 32);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 3)")), 3);
}

TEST_F(ImReduceHazeTest, DefaultLValue)
{
    eval("[B, T, L] = imreducehaze(A);");
    EXPECT_EQ(eval("class(L)").toString(), "double");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(L)")), 3);
    // For this saturated-haze input, L estimates to [1 1 1].
    EXPECT_NEAR(evalScalar("L(1)"), 1.0, 1e-6);
    EXPECT_NEAR(evalScalar("L(2)"), 1.0, 1e-6);
    EXPECT_NEAR(evalScalar("L(3)"), 1.0, 1e-6);
}

TEST_F(ImReduceHazeTest, DefaultPixelExact)
{
    eval("[B, T, L] = imreducehaze(A);");
    // Bit-exact with MATLAB on probed pixels.
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(8,8,1))")), 54);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(8,8,2))")), 146);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(8,8,3))")), 255);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(16,16,1))")), 141);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(16,16,2))")), 62);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(16,16,3))")), 255);
}

TEST_F(ImReduceHazeTest, DefaultThicknessExact)
{
    eval("[B, T, L] = imreducehaze(A);");
    EXPECT_EQ(eval("class(T)").toString(), "double");
    EXPECT_NEAR(evalScalar("T(8,8)"),   0.572982, 1e-5);
    EXPECT_NEAR(evalScalar("T(16,16)"), 0.560877, 1e-5);
}

// ── amount=0 passthrough ────────────────────────────────────────

TEST_F(ImReduceHazeTest, AmountZeroPassthrough)
{
    eval("[B, T, L] = imreducehaze(A, 0);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(isequal(B, A))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(isempty(T))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(isempty(L))")), 1);
}

// ── ContrastEnhancement variants ────────────────────────────────

TEST_F(ImReduceHazeTest, ContrastNoneExact)
{
    eval("B = imreducehaze(A, 1, 'ContrastEnhancement', 'none');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(8,8,1))")), 32);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(8,8,2))")), 121);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(8,8,3))")), 255);
}

TEST_F(ImReduceHazeTest, ContrastBoostRuns)
{
    // 'boost' uses the haze thickness map to per-pixel-gain the
    // dehazed image. Numerical match is sensitive to internal
    // ordering; we just verify it produces valid uint8 output.
    eval("B = imreducehaze(A, 1, 'ContrastEnhancement', 'boost');");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_GT(static_cast<int>(evalScalar("double(B(8,8,3))")), 200);
}

// ── Method=approxdcp (approximate parity) ──────────────────────

TEST_F(ImReduceHazeTest, ApproxDCPRuns)
{
    eval("[B, T, L] = imreducehaze(A, 1, 'Method', 'approxdcp');");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 3)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(L)")), 3);
}

// ── Explicit AtmosphericLight (bit-exact) ──────────────────────

TEST_F(ImReduceHazeTest, ExplicitAtmLightExact)
{
    eval("[B, T, L] = imreducehaze(A, 1, 'AtmosphericLight', [0.9 0.9 0.9]);");
    EXPECT_NEAR(evalScalar("L(1)"), 0.9, 1e-6);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(8,8,1))")), 76);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(8,8,2))")), 165);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(8,8,3))")), 255);
}

// ── Grayscale input ──────────────────────────────────────────────

TEST_F(ImReduceHazeTest, GrayscaleInput)
{
    eval("G = rgb2gray(A);"
         "[B, T, L] = imreducehaze(G);");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 32);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 2)")), 32);
    // 2-D output (no third dim).
    EXPECT_EQ(static_cast<int>(evalScalar("ndims(B)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(L)")), 1);
}

// ── single input class preservation ────────────────────────────

TEST_F(ImReduceHazeTest, SingleInputPreserved)
{
    eval("As = single(A) / 255;"
         "B = imreducehaze(As);");
    EXPECT_EQ(eval("class(B)").toString(), "single");
}

// ── Errors ──────────────────────────────────────────────────────

TEST_F(ImReduceHazeTest, AmountOutOfRangeThrows)
{
    EXPECT_THROW(eval("imreducehaze(A, -0.1);"),     std::exception);
    EXPECT_THROW(eval("imreducehaze(A, 1.1);"),      std::exception);
}

TEST_F(ImReduceHazeTest, BadMethodThrows)
{
    EXPECT_THROW(eval("imreducehaze(A, 1, 'Method', 'unknown');"),
                 std::exception);
}

TEST_F(ImReduceHazeTest, BadContrastEnhancementThrows)
{
    EXPECT_THROW(eval("imreducehaze(A, 1, 'ContrastEnhancement', 'rgb');"),
                 std::exception);
}

TEST_F(ImReduceHazeTest, BoostAmountWithoutBoostThrows)
{
    EXPECT_THROW(eval("imreducehaze(A, 1, 'BoostAmount', 0.5);"),
                 std::exception);
}

TEST_F(ImReduceHazeTest, BadAtmLightSizeThrows)
{
    EXPECT_THROW(
        eval("imreducehaze(A, 1, 'AtmosphericLight', [0.5 0.5]);"),
        std::exception);
}
