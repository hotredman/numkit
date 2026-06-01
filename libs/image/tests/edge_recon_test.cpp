// libs/image/tests/edge_recon_test.cpp
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

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EdgeReconTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
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
