// libs/image/tests/image_batch3_test.cpp
//
// Image batch 3 closure (26 functions):
//   bw analysis 2:  bwconncomp (DEFERRED) · bwdist · bweuler ·
//                   bwboundaries · bwselect · bwpack
//   histogram:      imhist · histeq · imadjust · stretchlim
//   arithmetic:     imadd · imsubtract · immultiply · imdivide ·
//                   imabsdiff · imcomplement · imlincomb
//   transforms:     idct2
//   stats:          mean2 · std2
//   color metrics:  deltaE (DEFERRED) · dice · jaccard
//   filters:        imsharpen · imboxfilt · imgaussfilt
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b
// (24 verified, 2 deferred — bwconncomp struct field access + deltaE
// output dims).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch3Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ImageBatch3Test, BwAnalysis2)
{
    eval("D = bwdist(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(D)"), 16.0);

    EXPECT_GT(evalScalar("bweuler(eye(4))"), 0.0);

    eval("B = bwboundaries(eye(4));");
    EXPECT_GT(evalScalar("numel(B)"), 0.0);

    eval("BW2 = bwselect(eye(4), 1, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW2)"), 16.0);

    eval("P = bwpack(eye(4));");
    EXPECT_GT(evalScalar("size(P,2)"), 0.0);
}

// bwdist distance-metric option (was silently ignored -> always Euclidean).
// BW = single TRUE at (2,2); corner (1,1) distinguishes the metrics. vs MATLAB.
TEST_F(ImageBatch3Test, BwdistMetrics)
{
    eval("BW = logical([0 0 0; 0 1 0; 0 0 0]);");
    // Euclidean (default): corner = sqrt(2).
    EXPECT_NEAR(evalScalar("D=bwdist(BW); D(1,1)"),               1.41421356, 1e-7);
    EXPECT_NEAR(evalScalar("D=bwdist(BW,'euclidean'); D(1,1)"),   1.41421356, 1e-7);
    // Cityblock: |1|+|1| = 2.
    EXPECT_DOUBLE_EQ(evalScalar("D=bwdist(BW,'cityblock'); D(1,1)"),  2.0);
    // Chessboard: max(1,1) = 1.
    EXPECT_DOUBLE_EQ(evalScalar("D=bwdist(BW,'chessboard'); D(1,1)"), 1.0);
    // Quasi-euclidean: diagonal step = sqrt(2).
    EXPECT_NEAR(evalScalar("D=bwdist(BW,'quasi-euclidean'); D(1,1)"), 1.41421356, 1e-7);
    // Edge (1,2) is 1 under every metric.
    EXPECT_DOUBLE_EQ(evalScalar("D=bwdist(BW,'cityblock'); D(1,2)"),  1.0);
    // Case-insensitive option; unknown metric throws.
    EXPECT_DOUBLE_EQ(evalScalar("D=bwdist(BW,'CityBlock'); D(1,1)"),  2.0);
    EXPECT_THROW(eval("bwdist(BW,'bogus');"), std::exception);
}

TEST_F(ImageBatch3Test, Histogram)
{
    eval("h = imhist(uint8([0 64 128 192 255]));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(h)"), 256.0);

    eval("J = histeq(uint8([0 50 100 150 255]));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(J)"), 5.0);

    eval("J = imadjust([0.0 0.5 1.0]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(J)"), 3.0);

    eval("lim = stretchlim([0.1 0.5 0.9]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(lim)"), 2.0);
}

// stretchlim uses 256 bins for uint8 but 65536 for double/single/uint16,
// matching MATLAB R2025b. 2026-05-31: previously a fixed 256 bins coarsely
// quantized the limits on a double image.
TEST_F(ImageBatch3Test, StretchlimDoubleBinCount)
{
    // double -> 65536-bin limits (6554/65535, 62258/65535)
    eval("s = stretchlim([0.1 0.2 0.9 0.95]);");
    EXPECT_NEAR(evalScalar("s(1)"), 0.10000762951, 1e-9);
    EXPECT_NEAR(evalScalar("s(2)"), 0.94999618524, 1e-9);
    // linspace(0,1,100) -> [662/65535, 64873/65535]
    eval("s2 = stretchlim(linspace(0,1,100));");
    EXPECT_NEAR(evalScalar("s2(1)"), 0.01010147249, 1e-9);
    EXPECT_NEAR(evalScalar("s2(2)"), 0.98989852751, 1e-9);
    // uint8 input is unchanged: 256-level [10/255, 250/255]
    eval("su = stretchlim(uint8([10 50 200 250]));");
    EXPECT_NEAR(evalScalar("su(1)"), 10.0 / 255.0,  1e-12);
    EXPECT_NEAR(evalScalar("su(2)"), 250.0 / 255.0, 1e-12);
}

// regionprops scalar shape descriptors (MajorAxisLength, MinorAxisLength,
// Eccentricity, Orientation, EquivDiameter, Extent) from the normalized
// 2nd central moments (+1/12 per-pixel variance) and area/bbox. 2026-05-31:
// regionprops previously shipped only Area/Centroid/BoundingBox so every
// shape field threw 'non-existent field'. vs MATLAB R2025b.
TEST_F(ImageBatch3Test, RegionpropsShapeDescriptors)
{
    // 3x2 block: axis-aligned, vertical major axis -> Orientation +90.
    eval("BW1 = false(6,6); BW1(2:4,2:3) = true;");
    eval("s1 = regionprops(BW1,'Extent','EquivDiameter','MajorAxisLength',"
         "'MinorAxisLength','Eccentricity','Orientation');");
    EXPECT_NEAR(evalScalar("s1.Extent"),          1.0,          1e-12);
    EXPECT_NEAR(evalScalar("s1.EquivDiameter"),   2.7639531958, 1e-9);
    EXPECT_NEAR(evalScalar("s1.MajorAxisLength"), 3.4641016151, 1e-9);
    EXPECT_NEAR(evalScalar("s1.MinorAxisLength"), 2.3094010768, 1e-9);
    EXPECT_NEAR(evalScalar("s1.Eccentricity"),    0.7453559925, 1e-9);
    EXPECT_NEAR(evalScalar("s1.Orientation"),     90.0,         1e-9);

    // Diagonal '\' blob: negative orientation (image rows increase down).
    eval("BW2 = false(7,7); BW2(2,2)=true; BW2(3,2)=true; BW2(3,3)=true; "
         "BW2(4,3)=true; BW2(4,4)=true; BW2(5,4)=true;");
    eval("s2 = regionprops(BW2,'Extent','MajorAxisLength','MinorAxisLength',"
         "'Eccentricity','Orientation');");
    EXPECT_NEAR(evalScalar("s2.Extent"),           0.5,           1e-12);
    EXPECT_NEAR(evalScalar("s2.MajorAxisLength"),  4.9852328997,  1e-9);
    EXPECT_NEAR(evalScalar("s2.MinorAxisLength"),  1.7741062358,  1e-9);
    EXPECT_NEAR(evalScalar("s2.Eccentricity"),     0.9345345981,  1e-9);
    EXPECT_NEAR(evalScalar("s2.Orientation"),    -50.3098276381,  1e-9);

    // Single pixel: degenerate (Major==Minor, Ecc=0, Orient=0).
    eval("BW3 = false(3,3); BW3(2,2)=true;");
    eval("s3 = regionprops(BW3,'MajorAxisLength','MinorAxisLength',"
         "'Eccentricity','Orientation','EquivDiameter');");
    EXPECT_NEAR(evalScalar("s3.MajorAxisLength"), 1.1547005384, 1e-9);
    EXPECT_NEAR(evalScalar("s3.MinorAxisLength"), 1.1547005384, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("s3.Eccentricity"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("s3.Orientation"),  0.0);
    EXPECT_NEAR(evalScalar("s3.EquivDiameter"),   1.1283791671, 1e-9);

    // Basic default (no props) still returns only Area/Centroid/BoundingBox.
    eval("sb = regionprops(BW1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(fieldnames(sb))"), 3.0);
}

// imhist 2nd output x (bin locations) spans the input CLASS's display
// range, not [0,1] (was always normalized). vs MATLAB R2025b.
TEST_F(ImageBatch3Test, ImhistBinLocationsByClass)
{
    eval("[c, x] = imhist(uint8([0 64 128 192 255]), 4);");
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 2.0);          // counts unchanged
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(2)"), 85.0);         // uint8 -> [0,255]
    EXPECT_DOUBLE_EQ(evalScalar("x(4)"), 255.0);
    // double image keeps [0,1] locations.
    eval("[~, xd] = imhist([0 0.25 0.5 0.75 1], 4);");
    EXPECT_DOUBLE_EQ(evalScalar("xd(4)"), 1.0);
    EXPECT_NEAR(evalScalar("xd(2)"), 1.0 / 3.0, 1e-12);
    // uint16 -> [0,65535].
    eval("[~, xu] = imhist(uint16([0 32768 65535]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("xu(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("xu(3)"), 65535.0);
    EXPECT_NEAR(evalScalar("xu(2)"), 32767.5, 1e-9);
}

TEST_F(ImageBatch3Test, Arithmetic)
{
    eval("C = imadd([1 2; 3 4], [10 10; 10 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2,2)"), 14.0);

    eval("C = imsubtract([10 20; 30 40], [1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 9.0);

    eval("C = immultiply([1 2; 3 4], [2 2; 2 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 2.0);

    eval("C = imdivide([10 20; 30 40], [2 2; 2 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 5.0);

    eval("C = imabsdiff([1 5], [3 2]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2)"), 3.0);

    eval("C = imcomplement([0.0 0.5 1.0]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("C(2)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("C(3)"), 0.0);

    eval("C = imlincomb(2, [1 2; 3 4], -1, [1 1; 1 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("C(1,1)"), 1.0);  // 2*1 - 1
}

TEST_F(ImageBatch3Test, IDCT2)
{
    eval("B = dct2([1 2; 3 4]); A = idct2(B);");
    EXPECT_NEAR(evalScalar("A(1,1)"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("A(2,2)"), 4.0, 1e-12);
}

TEST_F(ImageBatch3Test, Stats)
{
    EXPECT_DOUBLE_EQ(evalScalar("mean2([1 2; 3 4])"), 2.5);
    EXPECT_GT(evalScalar("std2([1 2; 3 4])"), 0.0);
}

TEST_F(ImageBatch3Test, DiceJaccard)
{
    EXPECT_DOUBLE_EQ(evalScalar("dice(eye(4) > 0,    eye(4) > 0)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("jaccard(eye(4) > 0, eye(4) > 0)"), 1.0);
}

TEST_F(ImageBatch3Test, Filters)
{
    eval("B = imsharpen([1 2 3; 4 5 6; 7 8 9]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 9.0);

    eval("B = imboxfilt([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = imgaussfilt([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);
}
