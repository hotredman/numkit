// libs/image/tests/imresize3_test.cpp
//
// Regression guard for imresize3 — 3-D volumetric resampling with
// MATLAB-aligned kernel and boundary convention.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Imresize3Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("A = reshape(double(1:60), 3, 4, 5);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── scale=2 default (cubic, no AA) ───────────────────────────────

TEST_F(Imresize3Test, DefaultScale2Cubic)
{
    eval("B = imresize3(A, 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 6);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,3)")), 10);
    EXPECT_NEAR(evalScalar("B(1,1,1)"),    -0.5,    1e-12);
    EXPECT_NEAR(evalScalar("B(2,2,2)"),     3.875,  1e-12);
    EXPECT_NEAR(evalScalar("B(3,3,3)"),    12.625,  1e-12);
    EXPECT_NEAR(evalScalar("B(6,8,10)"),   61.5,    1e-12);
}

// ── nearest ──────────────────────────────────────────────────────

TEST_F(Imresize3Test, Scale2Nearest)
{
    eval("B = imresize3(A, 2, 'nearest');");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1,1,1)")),    1);
    EXPECT_EQ(static_cast<int>(evalScalar("B(3,3,3)")),   17);
    EXPECT_EQ(static_cast<int>(evalScalar("B(6,8,10)")),  60);
}

// ── linear / triangle (equivalent) ───────────────────────────────

TEST_F(Imresize3Test, Scale2Linear)
{
    eval("B = imresize3(A, 2, 'linear');");
    EXPECT_NEAR(evalScalar("B(1,1,1)"),    1.0,  1e-12);
    EXPECT_NEAR(evalScalar("B(3,3,3)"),   13.0,  1e-12);
    EXPECT_NEAR(evalScalar("B(6,8,10)"),  60.0,  1e-12);
}

TEST_F(Imresize3Test, Scale2Triangle)
{
    eval("B = imresize3(A, 2, 'triangle');");
    EXPECT_NEAR(evalScalar("B(3,3,3)"),  13.0, 1e-12);
}

// ── Lanczos kernels ──────────────────────────────────────────────

TEST_F(Imresize3Test, Scale2Lanczos2)
{
    eval("B = imresize3(A, 2, 'lanczos2');");
    EXPECT_NEAR(evalScalar("B(1,1,1)"), -0.6257077129, 1e-6);
    EXPECT_NEAR(evalScalar("B(3,3,3)"), 12.2135425222, 1e-6);
    EXPECT_NEAR(evalScalar("B(6,8,10)"), 61.6257077129, 1e-6);
}

TEST_F(Imresize3Test, Scale2Lanczos3)
{
    eval("B = imresize3(A, 2, 'lanczos3');");
    EXPECT_NEAR(evalScalar("B(1,1,1)"), -1.0206525779, 1e-6);
    EXPECT_NEAR(evalScalar("B(3,3,3)"), 12.5528737940, 1e-6);
    EXPECT_NEAR(evalScalar("B(6,8,10)"), 62.0206525779, 1e-6);
}

// ── shrink with antialiasing ─────────────────────────────────────

TEST_F(Imresize3Test, Shrink05CubicAA)
{
    eval("B = imresize3(A, 0.5);");          // default cubic + AA
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,3)")), 3);
    EXPECT_NEAR(evalScalar("B(1,1,1)"),  8.29296875, 1e-10);
    EXPECT_NEAR(evalScalar("B(2,2,3)"), 58.390625,   1e-10);
}

TEST_F(Imresize3Test, Shrink05Box)
{
    eval("B = imresize3(A, 0.5, 'box');");
    EXPECT_NEAR(evalScalar("B(1,1,1)"),  9.0,  1e-12);
    EXPECT_NEAR(evalScalar("B(2,2,3)"), 58.5,  1e-12);
}

TEST_F(Imresize3Test, Shrink05Linear)
{
    eval("B = imresize3(A, 0.5, 'linear');");
    EXPECT_NEAR(evalScalar("B(1,1,1)"), 11.0,   1e-12);
    EXPECT_NEAR(evalScalar("B(2,2,3)"), 54.875, 1e-12);
}

TEST_F(Imresize3Test, Shrink05CubicNoAA)
{
    eval("B = imresize3(A, 0.5, 'cubic', 'Antialiasing', false);");
    EXPECT_NEAR(evalScalar("B(1,1,1)"),  8.0,    1e-12);
    EXPECT_NEAR(evalScalar("B(2,2,3)"), 60.3125, 1e-12);
}

// ── size-vector vs scalar-scale distinction ──────────────────────
// MATLAB uses the user's scale in the u formula, NOT outLen/inLen,
// so the two paths give different values for non-integer ratios.

TEST_F(Imresize3Test, SizeVectorPathDiffersFromScalarScale)
{
    eval("B1 = imresize3(A, 0.5);");
    eval("B2 = imresize3(A, [2 2 3]);");
    EXPECT_NEAR(evalScalar("B1(1,1,1)"), 8.29296875,    1e-10);
    EXPECT_NEAR(evalScalar("B2(1,1,1)"), 6.10181411031, 1e-6);
}

TEST_F(Imresize3Test, SizeVectorExactIntegerScale)
{
    eval("B = imresize3(A, [6 8 10]);");
    EXPECT_NEAR(evalScalar("B(3,4,5)"), 26.4765625, 1e-10);
}

// ── 4×4×4 even cubic upsample ────────────────────────────────────

TEST_F(Imresize3Test, Even4x4x4CubicScale2)
{
    eval("A2 = reshape(double(1:64), 4, 4, 4); B = imresize3(A2, 2, 'linear');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 8);
    EXPECT_NEAR(evalScalar("B(4,4,4)"), 27.25, 1e-12);
    EXPECT_NEAR(evalScalar("B(1,1,1)"),  1.0,  1e-12);
}

// ── NV pairs ─────────────────────────────────────────────────────

TEST_F(Imresize3Test, ScaleNVScalar)
{
    eval("B = imresize3(A, 'Scale', 0.5);");
    EXPECT_NEAR(evalScalar("B(1,1,1)"), 8.29296875, 1e-10);
}

TEST_F(Imresize3Test, OutputSizeNV)
{
    eval("B = imresize3(A, 'OutputSize', [2 2 3]);");
    EXPECT_NEAR(evalScalar("B(1,1,1)"), 6.10181411031, 1e-6);
}

TEST_F(Imresize3Test, MethodNV)
{
    eval("B = imresize3(A, 2, 'Method', 'linear');");
    EXPECT_NEAR(evalScalar("B(3,3,3)"), 13.0, 1e-12);
}

// ── uint8 class preservation (cubic + AA, scale 2) ───────────────
// uint8 cubic ties at exact .5 may differ from MATLAB's fixed-point
// path by ±1 (e.g. 61.5 → MATLAB 61, lround 62). Use ±1 tolerance.

TEST_F(Imresize3Test, Uint8ClassPreserved)
{
    eval("A8 = uint8(reshape(1:60, 3, 4, 5)); B = imresize3(A8, 2);");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 6);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1,1))")),  0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3,3))")), 13);
}

// ── errors ────────────────────────────────────────────────────────

TEST_F(Imresize3Test, NegativeScaleThrows)
{
    EXPECT_THROW(eval("imresize3(A, -1);"), std::exception);
}

TEST_F(Imresize3Test, UnknownMethodThrows)
{
    EXPECT_THROW(eval("imresize3(A, 2, 'gibberish');"), std::exception);
}

TEST_F(Imresize3Test, BadSizeVectorThrows)
{
    EXPECT_THROW(eval("imresize3(A, [1 2]);"), std::exception);
}
