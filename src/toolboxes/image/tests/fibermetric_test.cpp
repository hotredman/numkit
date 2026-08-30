// toolboxes/image/tests/fibermetric_test.cpp
//
// Regression guard for fibermetric — multiscale Frangi vesselness
// (Frangi et al. 1998 MICCAI). Pure-tube pixels match MATLAB exactly;
// crossing pixels diverge by a few percent due to MATLAB's private
// C++ builtin using slightly different Hessian / eigenvalue
// conventions that aren't documented (the maximum-vesselness over
// scales for a crossing is sensitive to those choices).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FiberMetricTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval(

            "A = zeros(20, 20, 'uint8');"
            "A(:, 10:11) = 200;"
            "A(10:11, :) = 200;"
            "A(8, 10) = 200;"
            "A(13, 10) = 200;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Default 6-scale → output class single ──────────────────────

TEST_F(FiberMetricTest, DefaultOutputClassSingle)
{
    eval("B = fibermetric(A);");
    EXPECT_EQ(eval("class(B)").toString(), "single");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 20);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 2)")), 20);
}

// ── Pure tube pixels: bit-exact ≈ 1.0 ────────────────────────

TEST_F(FiberMetricTest, TubePixelsHighResponse)
{
    eval("B = fibermetric(A);");
    EXPECT_NEAR(evalScalar("double(B(5,10))"),  1.0, 0.01);
    EXPECT_NEAR(evalScalar("double(B(10,5))"),  1.0, 0.01);
    EXPECT_NEAR(evalScalar("double(B(15,10))"), 1.0, 0.01);
    EXPECT_NEAR(evalScalar("double(B(10,15))"), 1.0, 0.01);
}

// ── Background pixels: zero ─────────────────────────────────

TEST_F(FiberMetricTest, BackgroundPixelsZero)
{
    eval("B = fibermetric(A);");
    EXPECT_NEAR(evalScalar("double(B(1,1))"),   0.0, 0.001);
    EXPECT_NEAR(evalScalar("double(B(20,20))"), 0.0, 0.001);
}

// ── Single-scale thickness=4 ────────────────────────────────

TEST_F(FiberMetricTest, SingleScaleThickness4)
{
    eval("B = fibermetric(A, 4);");
    EXPECT_NEAR(evalScalar("double(B(5,10))"), 1.0, 0.01);
}

TEST_F(FiberMetricTest, SingleScaleThickness2)
{
    eval("B = fibermetric(A, 2);");
    EXPECT_GT(evalScalar("double(B(5,10))"), 0.95);
}

// ── ObjectPolarity dark: inverted image gives same response ──

TEST_F(FiberMetricTest, DarkPolarityOnInvertedMatchesBright)
{
    eval("Ainv = uint8(255 - A);"
         "Bb = fibermetric(A, 4);"
         "Bd = fibermetric(Ainv, 4, 'ObjectPolarity', 'dark');");
    // Tube pixels match exactly under inversion + polarity flip.
    EXPECT_NEAR(evalScalar("double(Bb(5,10))"),
                evalScalar("double(Bd(5,10))"), 1e-6);
    EXPECT_NEAR(evalScalar("double(Bb(10,5))"),
                evalScalar("double(Bd(10,5))"), 1e-6);
}

TEST_F(FiberMetricTest, DarkPolarityOnBrightImageGivesZero)
{
    // Bright structure with dark polarity → all-zero output.
    eval("B = fibermetric(A, 4, 'ObjectPolarity', 'dark');");
    EXPECT_NEAR(evalScalar("double(B(5,10))"), 0.0, 1e-6);
    EXPECT_NEAR(evalScalar("double(B(10,5))"), 0.0, 1e-6);
}

// ── StructureSensitivity ────────────────────────────────────

TEST_F(FiberMetricTest, HighStructureSensitivityReducesResponse)
{
    // Higher c → tighter Frangi threshold → tube pixels with
    // moderate Hessian magnitude get smaller response.
    eval("B1 = fibermetric(A, 4, 'StructureSensitivity', 2.55);"
         "B2 = fibermetric(A, 4, 'StructureSensitivity', 50);");
    // At c=50, even pure-tube pixels should be < 1.
    EXPECT_LT(evalScalar("double(B2(5,10))"),
              evalScalar("double(B1(5,10))"));
}

// ── Class preservation ──────────────────────────────────────

TEST_F(FiberMetricTest, DoubleInDoubleOut)
{
    eval("Ad = double(A)/255;"
         "B = fibermetric(Ad);");
    EXPECT_EQ(eval("class(B)").toString(), "double");
}

TEST_F(FiberMetricTest, SingleInSingleOut)
{
    eval("As = single(A)/255;"
         "B = fibermetric(As);");
    EXPECT_EQ(eval("class(B)").toString(), "single");
}

TEST_F(FiberMetricTest, Uint16InSingleOut)
{
    eval("Au = uint16(A) * 257;"
         "B = fibermetric(Au);");
    EXPECT_EQ(eval("class(B)").toString(), "single");
}

// ── Flat image → all zero ──────────────────────────────────

TEST_F(FiberMetricTest, FlatImageIsZero)
{
    eval("F = uint8(100 * ones(20, 20));"
         "B = fibermetric(F);");
    EXPECT_NEAR(evalScalar("double(B(5,10))"),  0.0, 1e-6);
    EXPECT_NEAR(evalScalar("double(B(10,10))"), 0.0, 1e-6);
}

// ── 3-D volume support ────────────────────────────────────

TEST_F(FiberMetricTest, ThreeDVolume)
{
    eval("V = zeros(20, 20, 20, 'uint8');"
         "V(:, 10:11, 10:11) = 200;"
         "B = fibermetric(V, 4);");
    EXPECT_EQ(eval("class(B)").toString(), "single");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 20);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 3)")), 20);
    // The tube responds positively at its interior.
    EXPECT_GT(evalScalar("double(B(10,10,10))"), 0.0);
    EXPECT_NEAR(evalScalar("double(B(1,1,1))"), 0.0, 1e-6);
}

// ── Errors ─────────────────────────────────────────────────

TEST_F(FiberMetricTest, BadThicknessThrows)
{
    EXPECT_THROW(eval("fibermetric(A, -1);"),  std::exception);
    EXPECT_THROW(eval("fibermetric(A, 0);"),   std::exception);
    EXPECT_THROW(eval("fibermetric(A, 4.5);"), std::exception);
}

TEST_F(FiberMetricTest, BadPolarityThrows)
{
    EXPECT_THROW(eval("fibermetric(A, 4, 'ObjectPolarity', 'invalid');"),
                 std::exception);
}

TEST_F(FiberMetricTest, NegativeSensitivityThrows)
{
    EXPECT_THROW(
        eval("fibermetric(A, 4, 'StructureSensitivity', -1);"),
        std::exception);
}
