// toolboxes/image/tests/image_batch3_test.cpp
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

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch3Test : public ::testing::Test
{
public:
    StandardEngine engine;
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

// bwboundaries 2nd/3rd outputs [B,L,N]: label matrix + object count.
// 2026-05-31: only B was returned; L/N threw 'undefined function L'.
// numkit is noholes-mode, so L/N match MATLAB for hole-free inputs. vs R2025b.
TEST_F(ImageBatch3Test, BwboundariesLabelOutputs)
{
    eval("BW = false(5,5); BW(2:4,2:4) = true;");   // solid block, one object
    eval("[B, L, N] = bwboundaries(BW);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("N"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(L(:))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(L,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(L,2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("L(3,3)"), 1.0);   // interior labelled
    EXPECT_DOUBLE_EQ(evalScalar("L(1,1)"), 0.0);   // background
    // Two objects: each gets its own label; N counts objects.
    eval("BW2 = false(5,5); BW2(1,1)=true; BW2(3,3)=true; BW2(3,4)=true; BW2(4,4)=true;");
    eval("[B2, L2, N2] = bwboundaries(BW2);");
    EXPECT_DOUBLE_EQ(evalScalar("N2"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(B2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("L2(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("L2(4,4)"), 2.0);
    // A string mode flag is accepted (and ignored).
    EXPECT_NO_THROW(eval("bwboundaries(BW, 'noholes');"));
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

// bwdist 2nd output IDX (feature transform): nearest-foreground linear index,
// uint32, ties to lowest index. Was missing ('Index exceeds array dimensions').
TEST_F(ImageBatch3Test, BwdistFeatureIndex)
{
    eval("BW = logical([0 0 0 0; 0 1 0 0; 0 0 0 0; 0 0 0 1]);");  // seeds 6,16
    eval("[D, IDX] = bwdist(BW);");
    EXPECT_DOUBLE_EQ(evalScalar("double(strcmp(class(IDX),'uint32'))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(IDX(1,1))"),  6.0);  // nearest seed (2,2)
    EXPECT_DOUBLE_EQ(evalScalar("double(IDX(2,2))"),  6.0);  // self (foreground)
    EXPECT_DOUBLE_EQ(evalScalar("double(IDX(4,4))"), 16.0);  // self seed (4,4)
    EXPECT_DOUBLE_EQ(evalScalar("double(IDX(3,4))"), 16.0);  // nearest (4,4)
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(IDX(:)))"), 126.0);

    // Same IDX shape under the integer metrics; sums match MATLAB.
    eval("[Dc, IDXc] = bwdist(BW,'cityblock');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(IDXc(:)))"), 126.0);
    eval("[Dk, IDXk] = bwdist(BW,'chessboard');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(double(IDXk(:)))"), 126.0);

    // A tie (equidistant to two seeds) resolves to the LOWER linear index.
    eval("BW2 = false(5,5); BW2(1,1) = true; BW2(5,5) = true;");
    eval("[D2, IDX2] = bwdist(BW2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(IDX2(1,5))"),  1.0);  // tie -> idx 1
    EXPECT_DOUBLE_EQ(evalScalar("double(IDX2(5,5))"), 25.0);
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

// imadjust with an explicitly-passed empty [] for the in/out range means
// MATLAB's default [0 1] (identity, NO stretch) — distinct from the ABSENT
// 1-arg form imadjust(I), which auto-stretches via stretchlim. numkit
// previously treated empty [] like the absent case, contrast-stretching
// imadjust(I,[],[]). DEEP-PROBE c169.
TEST_F(ImageBatch3Test, ImadjustEmptyRangeIsIdentity)
{
    eval("I = uint8([10 50 90; 130 170 210; 30 70 110]);");
    eval("J1 = imadjust(I, [], []);");      // both empty -> identity
    eval("J2 = imadjust(I, [], [0 1]);");   // empty in-range
    eval("J3 = imadjust(I, [0 1], []);");   // empty out-range
    EXPECT_DOUBLE_EQ(evalScalar("double(isequal(J1, I))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isequal(J2, I))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isequal(J3, I))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(J1(1,1))"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(J1(2,2))"), 170.0);

    // 1-arg form still auto-stretches (stretchlim) — MATLAB J(2,2) = 204.
    eval("J4 = imadjust(I);");
    EXPECT_DOUBLE_EQ(evalScalar("double(isequal(J4, I))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(J4(2,2))"), 204.0);

    // explicit range + gamma still applies — MATLAB J5(2,2) = 154.
    eval("J5 = imadjust(I, [0.2 0.8], [0 1], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(J5(2,2))"), 154.0);
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

// regionprops PixelIdxList (column-major 1-based linear indices, sorted
// column vector) + PixelList (Px2 [col row], same order). 2026-05-31:
// both fields threw 'non-existent field'. vs MATLAB R2025b.
TEST_F(ImageBatch3Test, RegionpropsPixelLists)
{
    eval("BW = false(4,4); BW(2,2)=true; BW(2,3)=true; BW(3,3)=true;");
    eval("s = regionprops(BW,'PixelIdxList','PixelList');");
    // PixelIdxList: 3x1 column vector [6;10;11].
    EXPECT_DOUBLE_EQ(evalScalar("numel(s.PixelIdxList)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(s.PixelIdxList,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.PixelIdxList(1)"),  6.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.PixelIdxList(2)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.PixelIdxList(3)"), 11.0);
    // PixelList: 3x2 [x y]=[col row] -> [2 2; 3 2; 3 3].
    EXPECT_DOUBLE_EQ(evalScalar("size(s.PixelList,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(s.PixelList,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.PixelList(1,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.PixelList(1,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.PixelList(2,1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.PixelList(3,2)"), 3.0);
    // Second region's indices, sorted column-major.
    eval("B2 = false(5,5); B2(1,1)=true; B2(3,3)=true; B2(3,4)=true; B2(4,4)=true;");
    eval("s2 = regionprops(B2,'PixelIdxList');");
    EXPECT_DOUBLE_EQ(evalScalar("numel(s2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("s2(2).PixelIdxList(1)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("s2(2).PixelIdxList(3)"), 19.0);
    // Not part of the basic default set.
    eval("sb = regionprops(BW);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(fieldnames(sb))"), 3.0);
}

// regionprops intensity-image form: regionprops(BW, I, props) computes the
// intensity measurements. 2026-05-31: a numeric 2nd arg threw 'property
// names must be strings'. vs MATLAB R2025b.
TEST_F(ImageBatch3Test, RegionpropsIntensity)
{
    eval("BW = false(4,4); BW(2,2)=true; BW(2,3)=true; BW(3,3)=true;");
    eval("I = reshape(1:16,4,4);");
    eval("s = regionprops(BW, I, 'MeanIntensity','MaxIntensity','MinIntensity',"
         "'WeightedCentroid','PixelValues');");
    EXPECT_DOUBLE_EQ(evalScalar("s.MeanIntensity"), 9.0);   // mean([6 10 11])
    EXPECT_DOUBLE_EQ(evalScalar("s.MaxIntensity"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.MinIntensity"),  6.0);
    EXPECT_NEAR(evalScalar("s.WeightedCentroid(1)"), 75.0 / 27.0, 1e-9);
    EXPECT_NEAR(evalScalar("s.WeightedCentroid(2)"), 65.0 / 27.0, 1e-9);
    EXPECT_DOUBLE_EQ(evalScalar("numel(s.PixelValues)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(s.PixelValues,2)"), 1.0);  // column
    EXPECT_DOUBLE_EQ(evalScalar("s.PixelValues(1)"),  6.0);
    EXPECT_DOUBLE_EQ(evalScalar("s.PixelValues(3)"), 11.0);
    // uint8 intensity image.
    eval("I8 = uint8(reshape(0:10:150,4,4)); s8 = regionprops(BW,I8,'MeanIntensity');");
    EXPECT_DOUBLE_EQ(evalScalar("s8.MeanIntensity"), 80.0);
    // A string 2nd arg is still a property name, not an intensity image.
    eval("sa = regionprops(BW,'Area');");
    EXPECT_DOUBLE_EQ(evalScalar("sa.Area"), 3.0);
    // Intensity fields are not part of the basic default set.
    eval("sd = regionprops(BW, I);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(fieldnames(sd))"), 3.0);
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
    // Value guard for the DOUBLE/threshold==0 fused path (verified vs MATLAB
    // R2025b): unsharp mask leaves the centre pixel and total sum unchanged,
    // corners get the symmetric high-pass.
    EXPECT_NEAR(evalScalar("B(1,1)"),  -0.1301718755, 1e-9);
    EXPECT_NEAR(evalScalar("B(2,2)"),   5.0,          1e-12);
    EXPECT_NEAR(evalScalar("B(3,3)"),  10.1301718755, 1e-9);
    EXPECT_NEAR(evalScalar("sum(B(:))"), 45.0,        1e-9);
    // Generic (non-double) path with thresholding: stays uint8; centre is
    // below threshold so it is left unchanged (verified vs MATLAB R2025b).
    eval("C = imsharpen(uint8([10 20 30; 40 50 60; 70 80 90]), 'Threshold', 0.2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(C(2,2))"), 50.0);

    eval("B = imboxfilt([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);

    eval("B = imgaussfilt([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 4.0);
}
