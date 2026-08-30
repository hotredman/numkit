// toolboxes/image/tests/image_batch5_test.cpp
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

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch5Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
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

// DEEP-PROBE c177: imquantize [quant, index] 2nd output + the 'values' arg.
// index is the 1-based bin (1..N+1); quant = values(index) when 'values' is
// given, else quant == index. numkit previously returned only quant and
// threw on the 2nd output / ignored 'values'. vs MATLAB R2025b.
TEST_F(ImageBatch5Test, ImquantizeIndexAndValues)
{
    eval("I = [1 5 10; 15 20 25];");
    eval("[Q, idx] = imquantize(I, [8 18]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(isequal(Q, idx))"), 1.0);  // no values -> Q==idx
    EXPECT_DOUBLE_EQ(evalScalar("Q(1,1)"), 1.0);   // I=1  < 8   -> bin 1
    EXPECT_DOUBLE_EQ(evalScalar("Q(2,2)"), 3.0);   // I=20 >= 18 -> bin 3
    EXPECT_DOUBLE_EQ(evalScalar("idx(2,3)"), 3.0); // I=25 >= 18 -> bin 3
    // 'values' maps quant through the value table; index stays the bin.
    eval("[Qv, idxv] = imquantize(I, [8 18], [10 20 30]);");
    EXPECT_DOUBLE_EQ(evalScalar("Qv(1,1)"), 10.0);  // values(1)
    EXPECT_DOUBLE_EQ(evalScalar("Qv(2,2)"), 30.0);  // values(3)
    EXPECT_DOUBLE_EQ(evalScalar("double(isequal(idxv, idx))"), 1.0);
    // A wrong-size 'values' errors.
    EXPECT_THROW(eval("imquantize(I, [8 18], [1 2]);"), std::exception);
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

// imhistmatch == histeq(I, imhist(ref,nbins)); transform built at the
// input class's full resolution (NPTS=256 for uint8), nbins default 64,
// plus the 2nd output hgram (= imhist(ref,nbins)). Pinned to MATLAB R2025b.
TEST_F(ImageBatch5Test, ImhistmatchHgramOutput)
{
    eval("A = uint8([10 40 70 100; 130 160 190 220; 5 15 25 35; 200 210 230 250]);");
    eval("R = uint8([0 0 50 50; 100 100 150 150; 200 200 255 255; 60 60 90 90]);");

    // Default nbins = 64 (all classes). J matches MATLAB exactly.
    eval("[J, hg] = imhistmatch(A, R);");
    EXPECT_DOUBLE_EQ(evalScalar("double(J(1))"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(J(2))"), 101.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(J(4))"), 150.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(J(16))"), 255.0);

    // 2nd output hgram = imhist(R,64): 1x64 double row, sum == numel(R).
    EXPECT_DOUBLE_EQ(evalScalar("size(hg,1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(hg,2)"), 64.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(hg)"),    16.0);
    EXPECT_DOUBLE_EQ(evalScalar("hg(1)"),       2.0);
    EXPECT_DOUBLE_EQ(evalScalar("hg(13)"),      2.0);
    EXPECT_DOUBLE_EQ(evalScalar("hg(64)"),      2.0);

    // Explicit nbins = 16: finer target resolution, J + hgram both match.
    eval("[J16, hg16] = imhistmatch(A, R, 16);");
    EXPECT_DOUBLE_EQ(evalScalar("double(J16(1))"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(J16(4))"), 153.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(J16(16))"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(hg16)"), 16.0);
    EXPECT_DOUBLE_EQ(evalScalar("hg16(5)"),      2.0);

    // double image in [0,1], nbins = 8: output levels are k/(n-1) = k/7.
    eval("Ad = [0.1 0.4 0.7; 0.2 0.5 0.8; 0.05 0.35 0.95];");
    eval("Rd = [0 0.5 0.5; 0.5 1 1; 0 0.25 0.75];");
    eval("[Jd, hgd] = imhistmatch(Ad, Rd, 8);");
    EXPECT_NEAR(evalScalar("Jd(1)"), 0.0,                1e-12);
    EXPECT_NEAR(evalScalar("Jd(2)"), 2.0 / 7.0,          1e-12);
    EXPECT_NEAR(evalScalar("Jd(8)"), 1.0,                1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("sum(hgd)"), 9.0);
}
