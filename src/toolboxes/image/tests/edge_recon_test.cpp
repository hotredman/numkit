// toolboxes/image/tests/edge_recon_test.cpp
//
// Regression guards for two fixes driven by a customer script
// (DZ_po_stz.m):
//
//   * imreconstruct — rewritten from naive iterate-dilate-until-stable
//     (O(N·(H+W))) to the Vincent (1993) hybrid raster/anti-raster + FIFO
//     algorithm (O(N)). The fixed point is unchanged; these tests pin the
//     exact values (verified against MATLAB R2025b) so the speedup can't
//     silently change results.
//
//   * edge('Canny') — was a stub (raw gradient-magnitude threshold, no
//     non-max suppression, threshold not normalised → it marked ~half of
//     all pixels). Now a real Canny (derivative-of-Gaussian gradient +
//     interpolated NMS + double-threshold hysteresis). Also fixes a
//     case-sensitivity bug: method names are now matched case-insensitively
//     (MATLAB-compatible), so 'Canny' no longer silently fell through to
//     the default Sobel branch.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EdgeReconTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double sc(const std::string &c) { return eval(c).toScalar(); }
};

// ── imreconstruct (Vincent hybrid) — exact values vs MATLAB ───────────

// Grayscale reconstruction by dilation under a basin mask.
TEST_F(EdgeReconTest, ReconstructGrayscaleBasin)
{
    eval("mask = [10 10 10 10 10; 10 2 2 2 10; 10 2 8 2 10; 10 2 2 2 10; 10 10 10 10 10];");
    eval("marker = zeros(5,5); marker(3,3) = 8;");
    eval("R = imreconstruct(marker, mask);");
    EXPECT_DOUBLE_EQ(sc("sum(R(:))"), 56.0);
    EXPECT_DOUBLE_EQ(sc("max(R(:))"), 8.0);
    EXPECT_DOUBLE_EQ(sc("R(3,3)"), 8.0);
    EXPECT_DOUBLE_EQ(sc("R(1,1)"), 2.0);
}

// Reconstruction on a monotone ramp: the marker can only grow to the local
// floor, so it stays capped low away from the seed.
TEST_F(EdgeReconTest, ReconstructRamp)
{
    eval("m2 = uint8([1 2 3 4 5; 2 3 4 5 6; 3 4 5 6 7; 4 5 6 7 8; 5 6 7 8 9]);");
    eval("mk2 = zeros(5,5,'uint8'); mk2(1,1) = 9;");
    eval("R2 = imreconstruct(mk2, m2);");
    EXPECT_DOUBLE_EQ(sc("sum(R2(:))"), 25.0);
    EXPECT_DOUBLE_EQ(sc("R2(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(sc("R2(5,5)"), 1.0);
    EXPECT_EQ(eval("class(R2)").toString(), "uint8");
}

// imfill('holes') on a hollow ring — the interior hole must fill.
TEST_F(EdgeReconTest, ImfillRingHole)
{
    eval("BW = false(7,7);");
    eval("BW(2:6,2) = true; BW(2:6,6) = true; BW(2,2:6) = true; BW(6,2:6) = true;");
    eval("F = imfill(BW, 'holes');");
    EXPECT_DOUBLE_EQ(sc("nnz(BW)"), 16.0);
    EXPECT_DOUBLE_EQ(sc("nnz(F)"),  25.0);   // 5×5 block fully filled
}

// ── edge('Canny') — real detector, not the old stub ───────────────────

// A sharp square gives a THIN outline, not a filled/garbage blob. The old
// stub marked the whole gradient ramp (~half the pixels); a real Canny
// marks only the 1-px boundary (~perimeter), so nnz must be small relative
// to the object area and nonzero.
TEST_F(EdgeReconTest, CannyProducesThinOutline)
{
    eval("I = zeros(40,40); I(11:30,11:30) = 255; I = uint8(I);");
    eval("E = edge(I, 'Canny', 0.2);");
    EXPECT_EQ(eval("class(E)").toString(), "logical");
    const double n = sc("nnz(E)");
    EXPECT_GT(n, 40.0);     // there IS an outline (perimeter ≈ 4·20)
    EXPECT_LT(n, 200.0);    // but it's THIN — nowhere near the 400-px ramp / 1600-px fill
}

// Method names are case-insensitive (MATLAB-compatible): 'Canny' must not
// silently fall through to the Sobel default. All spellings agree.
TEST_F(EdgeReconTest, CannyCaseInsensitive)
{
    eval("I = zeros(40,40); I(11:30,11:30) = 255; I = uint8(I);");
    eval("a = edge(I, 'Canny', 0.2);");
    eval("b = edge(I, 'canny', 0.2);");
    eval("c = edge(I, 'CANNY', 0.2);");
    EXPECT_DOUBLE_EQ(sc("isequal(a, b)"), 1.0);
    EXPECT_DOUBLE_EQ(sc("isequal(a, c)"), 1.0);
}

// Canny on a uint8 image must give the same edges as on the double image.
// Previously the gradient was filtered in the input's integer type, which
// clamped the (signed) derivative to [0,255] — wiping out every FALLING
// edge (negative gradient). Here a bright bar has a rising edge on its left
// and a falling edge on its right; BOTH must be detected.
TEST_F(EdgeReconTest, CannyUint8FallingEdgeNotClamped)
{
    eval("I = zeros(30,30); I(:,11:20) = 200;");      // bright vertical bar
    eval("Eu = edge(uint8(I),  'Canny', 0.2);");
    eval("Ed = edge(double(I), 'Canny', 0.2);");
    EXPECT_DOUBLE_EQ(sc("isequal(Eu, Ed)"), 1.0);     // type-independent
    EXPECT_GT(sc("sum(sum(Eu(:,1:15)))"), 0.0);       // rising (left) edge
    EXPECT_GT(sc("sum(sum(Eu(:,16:30)))"), 0.0);      // falling (right) edge — lost if clamped
}

// ── imfill default connectivity = 4 (MATLAB) ──────────────────────────
//
// A diamond seals its interior only through diagonally-touching pixels.
// With 4-conn the background can't slip between them, so the interior is a
// hole and fills; with 8-conn it leaks to the border and doesn't. The
// DEFAULT must behave like conn=4 (it regressed to 8, which broke
// closed-contour fills like the customer's edge→imclose→imfill pipeline).
TEST_F(EdgeReconTest, ImfillDefaultConnIsFour)
{
    eval("BW = false(5,5);");
    eval("BW(1,3)=true; BW(2,2)=true; BW(2,4)=true; BW(3,1)=true; "
         "BW(3,5)=true; BW(4,2)=true; BW(4,4)=true; BW(5,3)=true;");  // 8-px diamond
    EXPECT_DOUBLE_EQ(sc("nnz(BW)"), 8.0);
    EXPECT_DOUBLE_EQ(sc("nnz(imfill(BW,'holes',4))"), 13.0);   // interior (5 px) filled
    EXPECT_DOUBLE_EQ(sc("nnz(imfill(BW,'holes',8))"),  8.0);   // leaks → nothing filled
    EXPECT_DOUBLE_EQ(sc("isequal(imfill(BW,'holes'), imfill(BW,'holes',4))"), 1.0);  // default == 4
}

// ── bwlabel label/count consistency ───────────────────────────────────
//
// Universal CCL invariant: the returned component count must equal both the
// max label and the number of distinct nonzero labels. A redundant second
// remap pass over already-remapped labels used to violate this on
// merge-heavy images (e.g. n=62 while max(L)=18), which corrupted
// regionprops / bwareaopen downstream.
// medfilt2 typed-access fast path (double/uint8) must be bit-identical to the
// elemAsDouble path. Pinned from MATLAB R2025b (zero-pad border).
TEST_F(EdgeReconTest, Medfilt2TypedFastPathPreserved)
{
    eval("I = reshape(mod((0:1199)*7.3,100),30,40);");
    eval("F = medfilt2(I);");
    EXPECT_NEAR(sc("sum(F(:))"), 57228.800000, 1e-3);
    EXPECT_NEAR(sc("F(15,20)"),  63.2, 1e-7);
    EXPECT_DOUBLE_EQ(sc("F(1,1)"), 0.0);                 // zero-pad corner
    eval("F5 = medfilt2(I,[5 5]);");
    EXPECT_NEAR(sc("F5(15,20)"), 51.5, 1e-7);
    eval("A = uint8(reshape(mod((0:1199)*11,256),30,40)); G = medfilt2(A);");
    EXPECT_EQ(eval("class(G)").toString(), "uint8");
    EXPECT_DOUBLE_EQ(sc("double(G(15,20))"),  98.0);      // uint8 → Huang path
    EXPECT_DOUBLE_EQ(sc("sum(double(G(:)))"), 143517.0);
    eval("G5 = medfilt2(A, [5 5]);");                      // larger odd window
    EXPECT_DOUBLE_EQ(sc("double(G5(15,20))"),  132.0);
    EXPECT_DOUBLE_EQ(sc("double(G5(1,1))"),    0.0);       // zero-pad corner
    EXPECT_DOUBLE_EQ(sc("sum(double(G5(:)))"), 136827.0);
    eval("H73 = medfilt2(A, [7 3]);");                     // non-square odd
    EXPECT_DOUBLE_EQ(sc("double(H73(15,20))"),  109.0);
    EXPECT_DOUBLE_EQ(sc("sum(double(H73(:)))"), 136284.0);
}

// imgradient with one output (magnitude) must skip the per-pixel atan2
// direction. Magnitude is bit-identical to the 2-output path and matches
// MATLAB. Pinned from MATLAB R2025b.
TEST_F(EdgeReconTest, ImgradientMagnitudeFastPath)
{
    eval("A = reshape(mod((0:1199)*7.3,100),30,40);");
    eval("m1 = imgradient(A);");          // nargout 1 → magnitude-only fast path
    EXPECT_NEAR(sc("sum(m1(:))"), 206327.486892, 1e-3);
    EXPECT_NEAR(sc("m1(15,20)"),  162.83292050, 1e-6);
    EXPECT_NEAR(sc("m1(1,1)"),    81.41646025, 1e-6);   // replicate border
    eval("[m2, d2] = imgradient(A);");    // nargout 2 → full path
    EXPECT_DOUBLE_EQ(sc("isequal(m1, m2)"), 1.0);       // magnitude identical
}

// stdfilt via two integral images (Σ, Σ²) for a rectangular neighbourhood
// must match the prior per-window result and MATLAB (both one-pass variance).
// Pinned from MATLAB R2025b. Symmetric border; always returns double.
TEST_F(EdgeReconTest, StdfiltIntegralPreserved)
{
    eval("I = reshape(mod((0:1199)*7.3, 100), 30, 40);");
    eval("F = stdfilt(I);");                              // default ones(3)
    EXPECT_NEAR(sc("sum(F(:))"), 30891.351644, 1e-2);
    EXPECT_NEAR(sc("F(15,20)"),  17.62718072, 1e-5);
    EXPECT_NEAR(sc("F(1,1)"),    10.17705753, 1e-5);      // symmetric border
    eval("F5 = stdfilt(I, ones(5));");
    EXPECT_NEAR(sc("F5(15,20)"), 28.82020414, 1e-5);
    eval("A = uint8(reshape(mod((0:1199)*11,256),30,40)); G = stdfilt(A);");
    EXPECT_EQ(eval("class(G)").toString(), "double");
    EXPECT_NEAR(sc("G(15,20)"),  79.83576893, 1e-5);
}

// Separable imboxfilt (uniform 1/k row ⊗ col == ones(k)/k²) must match the
// prior full-2D-kernel result and MATLAB. Pinned from MATLAB R2025b.
TEST_F(EdgeReconTest, ImboxfiltSeparablePreserved)
{
    eval("I = reshape(mod((0:1199)*7.3, 100), 30, 40);");
    eval("F = imboxfilt(I, 7);");
    EXPECT_NEAR(sc("sum(F(:))"), 59671.020408, 1e-3);
    EXPECT_NEAR(sc("F(15,20)"),  46.87346939, 1e-6);
    EXPECT_NEAR(sc("F(1,1)"),    22.54285714, 1e-6);   // replicate border
    eval("A = uint8(reshape(mod((0:1199)*11,256),30,40)); G = imboxfilt(A, 5);");
    EXPECT_EQ(eval("class(G)").toString(), "uint8");
    EXPECT_DOUBLE_EQ(sc("double(G(15,20))"),  126.0);
    EXPECT_DOUBLE_EQ(sc("sum(double(G(:)))"), 152187.0);
}

// Separable imgaussfilt must match the prior full-2D-kernel result (and
// MATLAB): the 2-D Gaussian is exactly the outer product of two 1-D
// Gaussians, so two 1-D passes give the same values (within FP summation
// order). Pinned from MATLAB R2025b (== prior numkit).
TEST_F(EdgeReconTest, ImgaussfiltSeparablePreserved)
{
    eval("I = reshape(mod((0:1199)*7.3, 100), 30, 40);");
    eval("F = imgaussfilt(I, 2.5);");
    EXPECT_NEAR(sc("sum(F(:))"), 59633.496960, 1e-3);
    EXPECT_NEAR(sc("F(15,20)"),  50.11185269, 1e-6);
    EXPECT_NEAR(sc("F(1,1)"),    23.01812735, 1e-6);   // replicate border
    EXPECT_NEAR(sc("F(30,40)"),  45.29753101, 1e-6);   // corner
    eval("A = uint8(reshape(mod((0:1199)*11, 256),30,40)); G = imgaussfilt(A, 1.5);");
    EXPECT_EQ(eval("class(G)").toString(), "uint8");
    EXPECT_DOUBLE_EQ(sc("double(G(15,20))"),  116.0);
    EXPECT_DOUBLE_EQ(sc("double(G(1,1))"),     47.0);
    EXPECT_DOUBLE_EQ(sc("sum(double(G(:)))"), 152140.0);
}

// Typed fast path for rgb2gray (uint8/double) must be bit-for-bit identical
// to the generic per-pixel reduction. Pinned from the pre-optimization engine.
TEST_F(EdgeReconTest, Rgb2grayFastPathPreserved)
{
    eval("A = uint8(reshape(mod((0:299)*7, 256), 10, 10, 3));");
    eval("g = rgb2gray(A);");
    EXPECT_EQ(eval("class(g)").toString(), "uint8");
    EXPECT_DOUBLE_EQ(sc("sum(g(:))"),        12398.0);
    EXPECT_DOUBLE_EQ(sc("double(g(1,1))"),   124.0);
    EXPECT_DOUBLE_EQ(sc("double(g(5,5))"),   176.0);
    EXPECT_DOUBLE_EQ(sc("double(g(10,10))"), 126.0);
    eval("D = double(A)/255; gd = rgb2gray(D);");
    EXPECT_NEAR(sc("sum(gd(:))"), 48.52789399, 1e-7);
    EXPECT_NEAR(sc("gd(5,5)"),    0.690378849, 1e-9);
}

// SIMD fast path for imfilter on DOUBLE input must be bit-for-bit identical
// to the scalar reduction (taps accumulated in the same order). Values
// pinned from the pre-optimization engine — interior + replicate borders.
TEST_F(EdgeReconTest, ImfilterDoubleFastPathPreserved)
{
    eval("I = double(reshape(mod((0:119)*13, 17), 10, 12));");
    eval("Fg = imfilter(I, fspecial('gaussian',[5 5],1.2));");
    EXPECT_NEAR(sc("sum(Fg(:))"),  815.2583255, 1e-6);
    EXPECT_NEAR(sc("Fg(5,6)"),     8.270905748, 1e-7);
    EXPECT_NEAR(sc("Fg(1,1)"),     3.009669115, 1e-7);   // border
    EXPECT_NEAR(sc("Fg(10,12)"),   2.655703831, 1e-7);   // border corner
    eval("Fa = imfilter(I, fspecial('average',3));");
    EXPECT_NEAR(sc("sum(Fa(:))"),  840.5555556, 1e-6);
    EXPECT_NEAR(sc("Fa(5,6)"),     8.777777778, 1e-7);
}

// SIMD fast path for flat-SE morphology on logical/uint8 must produce
// EXACTLY the generic reduction's result. Values pinned from the
// pre-optimization engine. (imdilate/imerode/imopen also match MATLAB here;
// imclose's border differs from MATLAB — that's a separate correctness item,
// not changed by this perf work.)
TEST_F(EdgeReconTest, MorphologyFlatSEPreserved)
{
    eval("bw = false(12,15); bw(3:9,4:11)=true; bw(1,1)=true; bw(12,15)=true;");
    eval("d = imdilate(bw, strel('disk',2));");
    EXPECT_DOUBLE_EQ(sc("nnz(d)"), 132.0);
    EXPECT_DOUBLE_EQ(sc("double(d(1,1))"), 1.0);   // border pixel dilated
    EXPECT_DOUBLE_EQ(sc("double(d(6,7))"), 1.0);
    eval("e = imerode(bw, strel('disk',2));");
    EXPECT_DOUBLE_EQ(sc("nnz(e)"), 12.0);
    EXPECT_DOUBLE_EQ(sc("double(e(3,4))"), 0.0);
    eval("c = imclose(bw, strel('disk',2));");
    EXPECT_DOUBLE_EQ(sc("nnz(c)"), 76.0);
    eval("o = imopen(bw, strel('square',3));");
    EXPECT_DOUBLE_EQ(sc("nnz(o)"), 56.0);
    // uint8 grayscale dilate/erode — square and disk SEs.
    eval("g = uint8(reshape(mod((0:179)*7,256),12,15));");
    eval("gd = imdilate(g, strel('square',3));");
    EXPECT_DOUBLE_EQ(sc("sum(gd(:))"), 38889.0);
    EXPECT_DOUBLE_EQ(sc("double(gd(1,1))"), 91.0);  // border
    eval("ge = imerode(g, strel('square',3));");
    EXPECT_DOUBLE_EQ(sc("sum(ge(:))"), 6406.0);
    EXPECT_DOUBLE_EQ(sc("double(ge(6,7))"), 20.0);
    eval("gdk = imdilate(g, strel('disk',3));");
    EXPECT_DOUBLE_EQ(sc("sum(gdk(:))"), 42232.0);
    EXPECT_DOUBLE_EQ(sc("double(gdk(6,6))"), 213.0);
}

TEST_F(EdgeReconTest, BwlabelCountMatchesLabels)
{
    // A "comb": three vertical stripes joined by a bottom bar (one
    // component, many label merges) plus one separate blob.
    eval("BW = false(6,10);");
    eval("BW(1:5,2)=true; BW(1:5,5)=true; BW(1:5,8)=true; BW(5,2:8)=true;");  // comb
    eval("BW(1:2,10)=true;");                                                  // separate blob
    eval("[L,n] = bwlabel(BW);");
    EXPECT_DOUBLE_EQ(sc("n"), 2.0);
    EXPECT_DOUBLE_EQ(sc("max(L(:))"), 2.0);
    EXPECT_DOUBLE_EQ(sc("n == max(L(:))"), 1.0);
    EXPECT_DOUBLE_EQ(sc("numel(unique(L(L>0)))"), 2.0);
}
