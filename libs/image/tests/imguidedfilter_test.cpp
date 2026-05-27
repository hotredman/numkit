// libs/image/tests/imguidedfilter_test.cpp
//
// Regression guard for imguidedfilter — Guided Image Filter (He
// et al. 2013). Bit-equal MATLAB R2025b at 1e-10 tolerance.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImguidedfilterTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval(
            "import compat.*;"
            "I = double(reshape(1:25, 5, 5)) / 25;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Default (self-guide, NHood=5, eps=0.01) ───────────────────────

TEST_F(ImguidedfilterTest, DefaultSelfGuide)
{
    eval("B = imguidedfilter(I);");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52, 1e-10);
    EXPECT_NEAR(evalScalar("B(1,1)"), 0.08373697289, 1e-9);
}

// ── Output shape preserved ────────────────────────────────────────

TEST_F(ImguidedfilterTest, ShapePreserved)
{
    eval("B = imguidedfilter(I);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B,2)")), 5);
}

// ── Scalar NeighborhoodSize ───────────────────────────────────────

TEST_F(ImguidedfilterTest, ScalarNHood)
{
    eval("B = imguidedfilter(I, 'NeighborhoodSize', 3);");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52, 1e-10);
}

// ── Custom DegreeOfSmoothing ──────────────────────────────────────

TEST_F(ImguidedfilterTest, CustomEps)
{
    eval("B = imguidedfilter(I, 'DegreeOfSmoothing', 0.01);");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52, 1e-10);
}

// ── Cross-guidance (G != A) ───────────────────────────────────────

TEST_F(ImguidedfilterTest, CrossGuidance)
{
    eval("G = double([0 0 0 0 0; 0 1 1 1 0; 0 1 1 1 0; 0 1 1 1 0; 0 0 0 0 0]);"
         "B = imguidedfilter(I, G);");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52, 1e-10);
}

// ── uint8 input class → uint8 output ──────────────────────────────

TEST_F(ImguidedfilterTest, Uint8Class)
{
    eval("Iu = uint8(I * 255); B = imguidedfilter(Iu);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3))")), 133);
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
}

// ── 2-element NeighborhoodSize (square only here) ─────────────────

TEST_F(ImguidedfilterTest, VectorNHoodSquare)
{
    eval("B = imguidedfilter(I, 'NeighborhoodSize', [3 3]);");
    EXPECT_NEAR(evalScalar("B(3,3)"), 0.52, 1e-10);
}

// ── Errors ─────────────────────────────────────────────────────────

TEST_F(ImguidedfilterTest, EvenNHoodThrows)
{
    EXPECT_THROW(eval("imguidedfilter(I, 'NeighborhoodSize', 4);"),
                 std::exception);
}

TEST_F(ImguidedfilterTest, NonSquareNHoodThrows)
{
    EXPECT_THROW(eval("imguidedfilter(I, 'NeighborhoodSize', [3 5]);"),
                 std::exception);
}

TEST_F(ImguidedfilterTest, NegativeEpsThrows)
{
    EXPECT_THROW(eval("imguidedfilter(I, 'DegreeOfSmoothing', -0.01);"),
                 std::exception);
}
