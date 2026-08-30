// toolboxes/image/tests/image_batch4_test.cpp
//
// Image batch 4 closure (28 functions):
//   regional/morph: regionprops · imregionalmax · imregionalmin ·
//                   imtophat · imbothat · strel (disk now MATLAB-parity)
//   conversion:     gray2ind · ind2rgb · grayslice · integralImage ·
//                   im2bw · im2col · im2gray · getrangefromclass
//   bw extras:      applylut · boundarymask · bwareafilt
//   stats:          entropy · entropyfilt · immse
//   transforms:     fft2 · ifft2 · freqz2 · fspecial
//   morph extras:   imhmax · imhmin
//   thresh:         graythresh (DEFERRED) · imbinarize (DEFERRED)
//
// 25 verified bit-identical MATLAB R2025b; 3 deferred.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImageBatch4Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ImageBatch4Test, RegionalMorph)
{
    eval("p = regionprops(eye(4));");
    EXPECT_GT(evalScalar("numel(p)"), 0.0);

    eval("BW = imregionalmax(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);

    eval("BW = imregionalmin(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);

    eval("B = imtophat(eye(5), ones(3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 25.0);

    eval("B = imbothat(eye(5), ones(3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 25.0);
}

// strel('disk',R) now matches MATLAB R2025b's radial periodic-line
// decomposition (default N=4), not a full Euclidean disk. Pinned to MATLAB.
TEST_F(ImageBatch4Test, StrelDisk)
{
    // r=5 default (N=4): 9x9, sum 69, symmetric octagon-ish profile.
    eval("se5 = strel('disk',5); nh5 = se5.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("size(nh5,1)"),  9.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(nh5,2)"),  9.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh5(:))"), 69.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh5(1,:))"), 5.0);   // top row clipped
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh5(3,:))"), 9.0);   // mid rows full width
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh5(:,1))"), 5.0);

    // r=3: decomposition degrades to a full 5x5 square (sum 25).
    eval("se3 = strel('disk',3); nh3 = se3.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("size(nh3,1)"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh3(:))"), 25.0);

    // r=4 -> 7x7/37, r=7 -> 13x13/157.
    eval("se4 = strel('disk',4); nh4 = se4.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh4(:))"), 37.0);
    eval("se7 = strel('disk',7); nh7 = se7.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("size(nh7,1)"), 13.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh7(:))"), 157.0);

    // N=0 forces the true Euclidean disk: r=5 -> 11x11/81.
    eval("se0 = strel('disk',5,0); nh0 = se0.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("size(nh0,1)"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh0(:))"), 81.0);

    // r<3 always Euclidean: r=2 -> 5x5/13 (matches MATLAB).
    eval("se2 = strel('disk',2); nh2 = se2.Neighborhood;");
    EXPECT_DOUBLE_EQ(evalScalar("sum(nh2(:))"), 13.0);
}

TEST_F(ImageBatch4Test, Conversion)
{
    eval("[X, map] = gray2ind([0.0 0.5 1.0], 8);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(X)"), 3.0);

    eval("rgb = ind2rgb([1 2 3], [1 0 0; 0 1 0; 0 0 1]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(rgb)"), 9.0);

    eval("BW = grayslice([0.1 0.5 0.9], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 3.0);

    eval("I = integralImage([1 2; 3 4]);");
    EXPECT_GT(evalScalar("numel(I)"), 0.0);

    eval("BW = im2bw([0.1 0.5 0.9], 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("double(BW(1))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(BW(3))"), 1.0);

    eval("g = im2gray(zeros(2,2,3));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(g)"), 4.0);

    eval("r = getrangefromclass(uint8(0));");
    EXPECT_DOUBLE_EQ(evalScalar("double(r(1))"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(r(2))"), 255.0);
}

TEST_F(ImageBatch4Test, BwExtras)
{
    eval("BW = applylut(eye(3), zeros(512,1));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 9.0);

    eval("BW = boundarymask(eye(4));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);

    eval("BW = bwareafilt(eye(4), 1);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(BW)"), 16.0);
}

TEST_F(ImageBatch4Test, ImageStats)
{
    EXPECT_GT(evalScalar("entropy(uint8([0 50 100 200 255]))"), 0.0);

    eval("J = entropyfilt(uint8([0 50; 100 200]));");
    EXPECT_DOUBLE_EQ(evalScalar("numel(J)"), 4.0);

    EXPECT_DOUBLE_EQ(evalScalar("immse([1 2 3], [1 2 3])"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("immse([1 2], [3 4])"),     4.0);  // mean((2)^2+(2)^2) = 4
}

TEST_F(ImageBatch4Test, Transforms2D)
{
    eval("F = fft2([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(evalScalar("real(F(1,1))"), 10.0);  // sum

    eval("A = ifft2(fft2([1 2; 3 4]));");
    EXPECT_NEAR(evalScalar("real(A(1,1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(A(2,2))"), 4.0, 1e-12);

    eval("h = freqz2([1 0; 0 1], 16, 16);");
    EXPECT_GT(evalScalar("numel(h)"), 0.0);

    eval("h = fspecial('average', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(h)"), 9.0);
}

// fspecial('disk',r) is the exact sub-pixel area-coverage integral (MATLAB
// R2025b), not a linear-taper approximation. Pinned to MATLAB.
TEST_F(ImageBatch4Test, FspecialDisk)
{
    eval("h2 = fspecial('disk', 2);");
    EXPECT_DOUBLE_EQ(evalScalar("size(h2,1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(h2,2)"), 5.0);
    EXPECT_NEAR(evalScalar("sum(h2(:))"), 1.0,            1e-12);
    EXPECT_NEAR(evalScalar("h2(3,3)"), 0.0795774715459,  1e-10);  // centre
    EXPECT_NEAR(evalScalar("h2(1,1)"), 0.0,              1e-12);  // corner
    EXPECT_NEAR(evalScalar("h2(3,1)"), 0.0381149714291,  1e-10);  // mid edge
    EXPECT_NEAR(evalScalar("h2(2,2)"), 0.0783813541665,  1e-10);

    eval("h3 = fspecial('disk', 3);");
    EXPECT_DOUBLE_EQ(evalScalar("size(h3,1)"), 7.0);
    EXPECT_NEAR(evalScalar("h3(4,4)"), 0.0353677651315,  1e-10);

    eval("h5 = fspecial('disk', 5);");
    EXPECT_DOUBLE_EQ(evalScalar("size(h5,1)"), 11.0);
    EXPECT_NEAR(evalScalar("sum(h5(:))"), 1.0,           1e-12);
    EXPECT_NEAR(evalScalar("h5(6,6)"), 0.0127323954474,  1e-10);
}

// fspecial('unsharp',alpha) = [-a a-1 -a; a-1 a+5 a-1; -a a-1 -a]/(a+1)
// (MATLAB R2025b). Previously threw 'unknown filter type'. Pinned to MATLAB.
TEST_F(ImageBatch4Test, FspecialUnsharp)
{
    eval("u = fspecial('unsharp');");  // default alpha = 0.2
    EXPECT_DOUBLE_EQ(evalScalar("numel(u)"), 9.0);
    EXPECT_NEAR(evalScalar("sum(u(:))"), 1.0,            1e-12);
    EXPECT_NEAR(evalScalar("u(2,2)"),  4.3333333333333,  1e-10);  // centre
    EXPECT_NEAR(evalScalar("u(1,2)"), -0.6666666666667,  1e-10);  // edge-mid
    EXPECT_NEAR(evalScalar("u(1,1)"), -0.1666666666667,  1e-10);  // corner

    eval("u5 = fspecial('unsharp', 0.5);");
    EXPECT_NEAR(evalScalar("sum(u5(:))"), 1.0,           1e-12);
    EXPECT_NEAR(evalScalar("u5(2,2)"),  3.6666666666667, 1e-10);
    EXPECT_NEAR(evalScalar("u5(1,2)"), -0.3333333333333, 1e-10);

    eval("u0 = fspecial('unsharp', 0);");
    EXPECT_NEAR(evalScalar("u0(2,2)"),  5.0,             1e-12);
    EXPECT_NEAR(evalScalar("u0(1,1)"),  0.0,             1e-12);
}

// fspecial('motion',len,theta) = anti-aliased motion-blur PSF (MATLAB
// R2025b). Was 'unknown filter type'; also fixes the fspecial_reg
// size-doubling bug that fed motion's theta the len value. Pinned to MATLAB.
TEST_F(ImageBatch4Test, FspecialMotion)
{
    // theta = 0: a horizontal line of len averaging weights (1 x len, all 1/len).
    eval("m0 = fspecial('motion', 9, 0);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m0,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m0,2)"), 9.0);
    EXPECT_NEAR(evalScalar("sum(m0(:))"), 1.0,        1e-12);
    EXPECT_NEAR(evalScalar("m0(1,1)"), 1.0 / 9.0,     1e-12);

    // theta = 90: vertical (9 x 1).
    eval("m90 = fspecial('motion', 9, 90);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m90,1)"), 9.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m90,2)"), 1.0);
    EXPECT_NEAR(evalScalar("sum(m90(:))"), 1.0,       1e-12);

    // theta = 45: a 7x7 anti-aliased diagonal.
    eval("m45 = fspecial('motion', 9, 45);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m45,1)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(m45,2)"), 7.0);
    EXPECT_NEAR(evalScalar("sum(m45(:))"), 1.0,       1e-12);
    EXPECT_NEAR(evalScalar("m45(4,4)"), 0.0997064915, 1e-9);  // centre
    EXPECT_NEAR(evalScalar("m45(1,7)"), 0.0755136399, 1e-9);
    EXPECT_NEAR(evalScalar("m45(1,1)"), 0.0,          1e-12); // empty corner

    eval("m5 = fspecial('motion', 5, 45);");
    EXPECT_DOUBLE_EQ(evalScalar("size(m5,1)"), 5.0);
    EXPECT_NEAR(evalScalar("m5(3,3)"), 0.1771490832, 1e-9);
}

TEST_F(ImageBatch4Test, Im2Col)
{
    eval("C = im2col([1 2; 3 4], [2 2], 'distinct');");
    EXPECT_GT(evalScalar("numel(C)"), 0.0);
}

TEST_F(ImageBatch4Test, ImHmaxImHmin)
{
    eval("B = imhmax([1 5 1; 1 1 1; 1 1 1], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 9.0);

    eval("B = imhmin([1 5 1; 1 1 1; 1 1 1], 3);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(B)"), 9.0);
}

// DEEP-PROBE c176: imbinarize 'global' / 'adaptive' method-string forms.
// 'adaptive' rewires to adaptthresh + the per-pixel binarize comparison.
// numkit previously misread 'adaptive' as a per-pixel threshold and threw.
// Deterministic 16x16 mod-product image; sums pinned to MATLAB R2025b.
TEST_F(ImageBatch4Test, ImbinarizeMethodStrings)
{
    eval("J = uint8(mod((1:16)' * (1:16), 256));");
    // 'global' == the default Otsu threshold.
    eval("bg = imbinarize(J, 'global'); bd = imbinarize(J);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(bg(:))"), 78.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(isequal(bg, bd))"), 1.0);
    // 'adaptive' (default Sensitivity 0.5).
    eval("ba = imbinarize(J, 'adaptive');");
    EXPECT_DOUBLE_EQ(evalScalar("sum(ba(:))"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(ba(8,8))"), 0.0);
    // 'adaptive' with a higher Sensitivity flags more foreground.
    eval("bs = imbinarize(J, 'adaptive', 'Sensitivity', 0.7);");
    EXPECT_DOUBLE_EQ(evalScalar("sum(bs(:))"), 224.0);
    // An unknown method errors.
    EXPECT_THROW(eval("imbinarize(J, 'bogus');"), std::exception);
}

// DEEP-PROBE c181: multithresh REWRITE. numkit binned float data over [0,1]
// (via imhist) and returned normalised/midpoint-of-means thresholds, so
// non-[0,1] data was wildly wrong (multithresh(reshape(1:36,6,6)) -> 0.49 vs
// MATLAB 18.43). Now matches MATLAB's getpdf + Otsu + map2OriginalScale.
// N=1/N=2 are bit-exact; N>=3 uses a global DP (MATLAB uses fminsearch).
// All values pinned to MATLAB R2025b.
TEST_F(ImageBatch4Test, MultithreshDataScale)
{
    // double input 1..36: thresholds in the DATA range, not normalised.
    eval("A = reshape(1:36,6,6);");
    EXPECT_NEAR(evalScalar("multithresh(A)"),    18.431373, 1e-6);  // N=1 default
    eval("[t2, em2] = multithresh(A,2);");
    EXPECT_NEAR(evalScalar("t2(1)"), 12.392157, 1e-6);
    EXPECT_NEAR(evalScalar("t2(2)"), 24.470588, 1e-6);
    EXPECT_NEAR(evalScalar("em2"),    0.889321, 1e-6);              // effectiveness
    eval("[t1, em1] = multithresh(A,1);");
    EXPECT_NEAR(evalScalar("em1"),    0.750205, 1e-6);
    // double in [0,1].
    eval("Ad = (1:36)/36;");
    eval("td = multithresh(Ad,2);");
    EXPECT_NEAR(evalScalar("td(1)"), 0.344227, 1e-6);
    EXPECT_NEAR(evalScalar("td(2)"), 0.679739, 1e-6);
}

// DEEP-PROBE c182: graythresh built its histogram with default_nbins
// (64 bins for floating-point, 65536 for uint16); MATLAB graythresh always
// uses NPTS=256, so float/uint16 levels were off (double [0,1] -> 0.507937
// vs MATLAB 0.513725). Now uses 256 bins. Pinned to MATLAB R2025b.
TEST_F(ImageBatch4Test, GraythreshBinCount)
{
    // uint8 already used 256 bins -> unchanged, bit-exact.
    eval("Iu = uint8([20 20 20 20 120 120 120 120 220 220 220 220]);");
    EXPECT_NEAR(evalScalar("graythresh(Iu)"), 0.468627, 1e-6);
    // double in [0,1]: 64 -> 256 bins fixes the level + EM.
    eval("B = [0.1 0.2 0.3 0.8 0.9; 0.15 0.25 0.85 0.95 0.05];");
    eval("[lb, eb] = graythresh(B);");
    EXPECT_NEAR(evalScalar("lb"), 0.549020, 1e-6);
    EXPECT_NEAR(evalScalar("eb"), 0.954264, 1e-6);   // effectiveness metric
    // uint16: 65536 -> 256 bins.
    eval("Cu = uint16([1000 2000 30000 40000 50000 60000 5000 8000]);");
    EXPECT_NEAR(evalScalar("graythresh(Cu)"), 0.288235, 1e-6);
}

TEST_F(ImageBatch4Test, MultithreshUint8)
{
    // Integer input: scaled to [0,1] by data range, result rounded back.
    eval("Au = uint8([10 50 90 130 170 210 250 30 70]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(multithresh(Au,1))"), 110.0);
    eval("tu = multithresh(Au,2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(tu(1))"), 110.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(tu(2))"), 190.0);
    // Three well-separated clusters: thresholds land between them.
    eval("Is = uint8([repmat(20,1,100), repmat(120,1,100), repmat(220,1,100)]);");
    eval("ts = multithresh(Is,2);");
    EXPECT_DOUBLE_EQ(evalScalar("double(ts(1)) + double(ts(2))"), 240.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(ts(1))"),  70.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(ts(2))"), 170.0);
}
