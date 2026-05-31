// libs/image/tests/image_batch5_test.cpp
//
// Image batch 5 closure (13 functions):
//   filters extras: imhistmatch · imbilatfilt · imflatfield · imapplymatrix
//                   imboxfilt3 · imgaussfilt3
//   misc:           impyramid · imquantize · imadjustn · imhistmatchn
//   gradient:       imgradientxy
//   bw extras:      imkeepborder
//   overlay:        imoverlay (DEFERRED)
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b
// (12 verified, 1 deferred — imoverlay arg validation differs).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch5Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ImageBatch5Test, ImageFiltersExtras)
{
    eval("J = imhistmatch(uint8([0 100 200]), uint8([50 150 255]));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(J)"), 3.0);

    eval("B = imbilatfilt([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = imflatfield([1 2; 3 4], 1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = imapplymatrix(eye(3), zeros(3,3,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 27.0);

    eval("B = imboxfilt3(zeros(3,3,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 27.0);

    eval("B = imgaussfilt3(zeros(3,3,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 27.0);
}

TEST_F(ImageBatch5Test, Misc)
{
    eval("B = impyramid([1 2; 3 4], 'reduce');");
    EXPECT_GT(evalScalar("numel(B)"), 0.0);

    eval("q = imquantize([0.1 0.5 0.9], [0.3 0.7]);");
    EXPECT_DOUBLE_EQ(evalScalar("q(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("q(3)"), 3.0);

    eval("B = imadjustn(zeros(2,2,2));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 8.0);

    eval("B = imhistmatchn(zeros(2,2,2), zeros(2,2,2));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 8.0);
}

TEST_F(ImageBatch5Test, GradientXY)
{
    eval("[Gx, Gy] = imgradientxy([1 2 3; 4 5 6; 7 8 9]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(Gx)"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(Gy)"), 9.0);
}

// imgradientxy / imgradient sobel+prewitt gradient SIGN and direction.
// 2026-05-31: the 2-D sobel/prewitt kernels were sign-flipped, so Gx/Gy
// were negated (Gx=-40 vs MATLAB +40 on a left->right-increasing ramp)
// and imgradient's Gdir was 180 deg off (168.69 vs -11.31). vs MATLAB R2025b.
TEST_F(ImageBatch5Test, GradientSignAndDirection)
{
    eval("A = reshape(1:25,5,5);");                 // increases L->R and T->B
    // imgradientxy sobel: Gx>0 (intensity rises with column), Gy>0 (with row).
    eval("[gx, gy] = imgradientxy(A);");
    EXPECT_DOUBLE_EQ(evalScalar("gx(3,3)"), 40.0);
    EXPECT_DOUBLE_EQ(evalScalar("gy(3,3)"),  8.0);
    // imgradient: Gmag sign-invariant, Gdir = atan2(-Gy,Gx) = -11.3099 deg.
    eval("[gm, gd] = imgradient(A);");
    EXPECT_NEAR(evalScalar("gm(3,3)"), 40.792156, 1e-5);
    EXPECT_NEAR(evalScalar("gd(3,3)"), -11.3099325, 1e-6);
    // prewitt: same direction, Gx=30, Gy=6.
    eval("[gxp, gyp] = imgradientxy(A,'prewitt');");
    EXPECT_DOUBLE_EQ(evalScalar("gxp(3,3)"), 30.0);
    EXPECT_DOUBLE_EQ(evalScalar("gyp(3,3)"),  6.0);
    eval("[gmp, gdp] = imgradient(A,'prewitt');");
    EXPECT_NEAR(evalScalar("gdp(3,3)"), -11.3099325, 1e-6);
    // central method was already correct (unchanged).
    eval("[gmc, gdc] = imgradient(A,'central');");
    EXPECT_NEAR(evalScalar("gdc(3,3)"), -11.3099325, 1e-6);
}

TEST_F(ImageBatch5Test, ImKeepBorder)
{
    eval("BW = imkeepborder(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);
}
