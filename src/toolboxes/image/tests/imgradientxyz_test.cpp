// toolboxes/image/tests/imgradientxyz_test.cpp
//
// Regression guard for imgradientxyz + imgradient3 — 3-D directional
// gradients and polar magnitude / azimuth / elevation. Reference values
// from MATLAB R2025b (Sobel uses the [1,3,3,1]-weighted 3x3x3 kernel,
// not naive [1,2,1] extension).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImgradientxyzTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;"
                    "V = double(reshape(1:60, 3, 4, 5));");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── imgradientxyz: sobel (default) ──────────────────────────────────

TEST_F(ImgradientxyzTest, SobelInterior)
{
    eval("[Gx, Gy, Gz] = imgradientxyz(V);");
    EXPECT_NEAR(evalScalar("Gx(2,2,2)"), 132.0, 1e-12);
    EXPECT_NEAR(evalScalar("Gy(2,2,2)"), 44.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gz(2,2,2)"), 528.0, 1e-12);
}

TEST_F(ImgradientxyzTest, SobelReplicateCorners)
{
    eval("[Gx, Gy, Gz] = imgradientxyz(V);");
    EXPECT_NEAR(evalScalar("Gx(1,1,1)"), 66.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gy(1,1,1)"), 22.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gz(1,1,1)"), 264.0, 1e-12);
    EXPECT_NEAR(evalScalar("Gx(3,4,5)"), 66.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gy(3,4,5)"), 22.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gz(3,4,5)"), 264.0, 1e-12);
}

// ── imgradientxyz: prewitt ──────────────────────────────────────────

TEST_F(ImgradientxyzTest, Prewitt)
{
    eval("[Gx, Gy, Gz] = imgradientxyz(V, 'prewitt');");
    EXPECT_NEAR(evalScalar("Gx(2,2,2)"), 54.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gy(2,2,2)"), 18.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gz(2,2,2)"), 216.0, 1e-12);
}

// ── imgradientxyz: central ──────────────────────────────────────────

TEST_F(ImgradientxyzTest, Central)
{
    eval("[Gx, Gy, Gz] = imgradientxyz(V, 'central');");
    EXPECT_NEAR(evalScalar("Gx(2,2,2)"), 3.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gy(2,2,2)"), 1.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gz(2,2,2)"), 12.0, 1e-12);
    // Corner: central uses forward diff at boundary.
    EXPECT_NEAR(evalScalar("Gx(1,1,1)"), 3.0,  1e-12);
}

// ── imgradientxyz: intermediate ─────────────────────────────────────

TEST_F(ImgradientxyzTest, Intermediate)
{
    eval("[Gx, Gy, Gz] = imgradientxyz(V, 'intermediate');");
    EXPECT_NEAR(evalScalar("Gx(2,2,2)"), 3.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gy(2,2,2)"), 1.0,  1e-12);
    EXPECT_NEAR(evalScalar("Gz(2,2,2)"), 12.0, 1e-12);
    // Trailing slice zero-padded.
    EXPECT_NEAR(evalScalar("Gx(3,4,5)"), 0.0, 1e-12);
}

// ── imgradient3 — sobel polar ───────────────────────────────────────

TEST_F(ImgradientxyzTest, Imgradient3Sobel)
{
    eval("[Gmag, Gaz, Gel] = imgradient3(V);");
    EXPECT_NEAR(evalScalar("Gmag(2,2,2)"), 546.02564044164554, 1e-6);
    EXPECT_NEAR(evalScalar("Gaz(2,2,2)"),  -18.434948822922017, 1e-9);
    EXPECT_NEAR(evalScalar("Gel(2,2,2)"),   75.236867376916152, 1e-9);
}

// ── imgradient3 — single-output form ────────────────────────────────

TEST_F(ImgradientxyzTest, Imgradient3SingleOutput)
{
    eval("Gmag = imgradient3(V);");
    EXPECT_NEAR(evalScalar("Gmag(2,2,2)"), 546.02564044164554, 1e-6);
}

// ── imgradient3 — from-grads form ───────────────────────────────────

TEST_F(ImgradientxyzTest, Imgradient3FromGrads)
{
    eval("[Gx, Gy, Gz] = imgradientxyz(V);"
         "[Gmag, Gaz, Gel] = imgradient3(Gx, Gy, Gz);");
    EXPECT_NEAR(evalScalar("Gmag(2,2,2)"), 546.02564044164554, 1e-6);
    EXPECT_NEAR(evalScalar("Gaz(2,2,2)"),  -18.434948822922017, 1e-9);
    EXPECT_NEAR(evalScalar("Gel(2,2,2)"),   75.236867376916152, 1e-9);
}
