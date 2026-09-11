// tests/graphics/figure_export_test.cpp
//
// saveas / exportgraphics / print — the engine-side figure export
// (portion 32). Dual-engine per the portion-close gate. Files are
// written through the engine fs into the temp area and checked for
// magic bytes / required content, then removed.

#include "dual_engine_fixture.hpp"

#include <numkit/figure/figure_render.hpp>

#include <cstdio>
#include <cstring>
#include <string>

using namespace m_test;

namespace {

class FigureExportTest : public DualEngineTest
{
protected:
    std::string tmpPath(const char *name) const
    {
        return (std::string)std::tmpnam(nullptr) + name;
    }
    bool fileStartsWith(const std::string &path, const char *magic)
    {
        FILE *f = std::fopen(path.c_str(), "rb");
        if (!f)
            return false;
        char buf[8] = {0};
        std::string m(magic);
        size_t n = std::fread(buf, 1, m.size(), f);
        std::fclose(f);
        return n == m.size() && std::string(buf, m.size()) == m;
    }
    void removeFile(const std::string &path) { std::remove(path.c_str()); }
};

} // namespace

TEST_P(FigureExportTest, SaveasPngWritesPng)
{
    std::string p = tmpPath("_nkexp.png");
    eval("x = 0:0.1:6; plot(x, sin(x)); title('t');");
    eval("saveas(gcf, '" + p + "');");
    EXPECT_TRUE(fileStartsWith(p, "\x89PNG")) << p;
    removeFile(p);
}

TEST_P(FigureExportTest, SaveasSvgWritesVector)
{
    std::string p = tmpPath("_nkexp.svg");
    eval("plot(1:5, [1 4 2 5 3]);");
    eval("saveas(gcf, '" + p + "', 'svg');");
    FILE *f = std::fopen(p.c_str(), "rb");
    ASSERT_NE(f, nullptr);
    std::string data;
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        data.append(buf, n);
    std::fclose(f);
    EXPECT_NE(data.find("<svg"), std::string::npos);
    EXPECT_NE(data.find("<polyline"), std::string::npos);
    removeFile(p);
}

TEST_P(FigureExportTest, ExportgraphicsWithHandleAndResolution)
{
    std::string p = tmpPath("_nkexp2.png");
    eval("f = figure(); plot(1:10, (1:10).^2);");
    eval("exportgraphics(f, '" + p + "', 'Resolution', 300);");
    EXPECT_TRUE(fileStartsWith(p, "\x89PNG"));
    removeFile(p);
}

TEST_P(FigureExportTest, PrintCommandFlagForm)
{
    std::string p = tmpPath("_nkexp3.png");
    eval("plot(1:5);");
    eval("print('-dpng', '-r150', '" + p + "');");
    EXPECT_TRUE(fileStartsWith(p, "\x89PNG"));
    removeFile(p);
}

TEST_P(FigureExportTest, UnsupportedFormatThrowsWithGap)
{
    EXPECT_THROW(eval("saveas(gcf, 'out.pdf');"), std::runtime_error);
    try {
        eval("saveas(gcf, 'out.pdf');");
    } catch (const std::runtime_error &e) {
        EXPECT_NE(std::string(e.what()).find("not supported"), std::string::npos);
    }
}

TEST_P(FigureExportTest, DeletedHandleErrors)
{
    eval("f = figure(); close(f);");
    EXPECT_THROW(eval("saveas(f, 'x.png');"), std::runtime_error);
}

// Raster font alignment guard: the 5x7 table must map each char to its
// OWN glyph. The original table had 92 of 95 rows — three dropped rows
// silently shifted every later glyph +3 ("time" rendered as "wlph").
// The static_assert in figure_render.hpp now guards the row COUNT; this
// guard pins glyph IDENTITY for distinctive shapes across the alphabet.
TEST_P(FigureExportTest, RasterFontGlyphAlignment)
{
    using numkit::figrender::font5x7;
    const std::uint8_t *t = font5x7('t');
    EXPECT_EQ(t[2], 0x3F) << "'t' must have its crossbar";
    const std::uint8_t *I = font5x7('I');
    const std::uint8_t expI[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
    EXPECT_EQ(0, std::memcmp(I, expI, 5)) << "'I' must be symmetric";
    // The +3 shift bug made font5x7('t') return the 'w' glyph
    // {0x3C,0x40,0x30,0x40,0x3C}; assert it is NOT that.
    const std::uint8_t expW[5] = {0x3C, 0x40, 0x30, 0x40, 0x3C};
    EXPECT_NE(0, std::memcmp(t, expW, 5)) << "'t' returned the 'w' glyph";
    // A digit and an uppercase letter after the historic gap region.
    EXPECT_EQ(font5x7('8')[0], 0x36);
    EXPECT_EQ(font5x7('A')[0], 0x7C);
}

INSTANTIATE_DUAL(FigureExportTest);

// Column-major guard (portion 33): decode the exported PNG back through
// imread and verify ACTUAL PIXELS — the geometry is deterministic
// (plot(1:5,3s) horizontal line at y=3 → pixel ≈ (445,301) must carry
// the palette green #7fd99a; background white; dims 630×840). A linear
// row-major→Value transfer scrambles exactly this and would fail here.
TEST_P(FigureExportTest, PngPixelsDecodeColumnMajor)
{
    std::string p = tmpPath("_nkpx.png");
    eval("plot([1 2], [3 3]);");   // no title → margins deterministic
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    EXPECT_NEAR(evalScalar("size(A,1)"), 630.0, 0.5);
    EXPECT_NEAR(evalScalar("size(A,2)"), 840.0, 0.5);
    EXPECT_NEAR(evalScalar("size(A,3)"), 3.0, 0.5);
    // White background well inside the canvas.
    EXPECT_GT(evalScalar("A(20, 400, 1)"), 250.0);
    EXPECT_GT(evalScalar("A(20, 400, 3)"), 250.0);
    // The series line: scan a ±6px window around (445,301) for the
    // palette green (127, 217, 154).
    bool found = false;
    for (int dy = -8; dy <= 8 && !found; ++dy)
        for (int dx = -8; dx <= 8 && !found; ++dx) {
            std::string px = "A(" + std::to_string(301 + dy) + ", "
                           + std::to_string(445 + dx) + ", :)";
            double r = evalScalar(px + "(1)"), g = evalScalar(px + "(2)"),
                   b = evalScalar(px + "(3)");
            if (r > 100 && r < 160 && g > 190 && g < 245 && b > 130
                && b < 180)
                found = true;
        }
    EXPECT_TRUE(found) << "palette-green line pixel not found near (445,301)";
    removeFile(p);
}

// ── Image export (portion 35: imagesc / imshow / colormap) ───────────
// All expected colors are MATLAB R2025b pixel-probed (2026-09-11): a
// scaled value v maps to palette entry round(t*(N-1))+1 with
// t=(v-clo)/(chi-clo) clamped; NaN renders as entry 1. numkit's 254-
// level quantization costs at most one adjacent palette entry → ±3 on
// each uint8 channel. Canvas layout is deterministic (840×630, single
// axes, margins ml=64/mr=14/mt=20/mb=48) → cell centers are computed,
// not scanned.

namespace {

// Sample A(y, x, :) from an exported PNG loaded via imread; returns
// false when the pixel is not within ±tol of the expected RGB.
::testing::AssertionResult pixelMatches(FigureExportTest *t,
                                        const std::string &a, int y, int x,
                                        int r, int g, int b, int tol)
{
    double pr = t->evalScalar(a + "(" + std::to_string(y) + ","
                              + std::to_string(x) + ",1)");
    double pg = t->evalScalar(a + "(" + std::to_string(y) + ","
                              + std::to_string(x) + ",2)");
    double pb = t->evalScalar(a + "(" + std::to_string(y) + ","
                              + std::to_string(x) + ",3)");
    if (std::abs(pr - r) <= tol && std::abs(pg - g) <= tol
        && std::abs(pb - b) <= tol)
        return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure()
           << "pixel(" << y << "," << x << ") = (" << pr << "," << pg << ","
           << pb << "), expected (" << r << "," << g << "," << b
           << ") ±" << tol;
}

// Cell-center pixel of the 3x4 default imagesc layout. plot rect:
// x∈[74,826], y∈[20,582] (no title); limits [0.5,4.5]x[0.5,3.5] with
// YDir reverse (row 1 at top).
int cellX(int col) { return 74 + (int)((col + 0.5) / 4.0 * 752.0); }
int cellY(int row) { return 20 + (int)((row + 0.5) / 3.0 * 562.0); }

} // namespace

// MATLAB probe p35_k: parula 256, CLim auto [1 12]. Every expected
// value below was read from the MATLAB-exported PNG at the same cell.
TEST_P(FigureExportTest, ImagescParulaPixelColors)
{
    std::string p = tmpPath("_nk_img.png");
    eval("C = [1 2 3 4; 5 6 7 8; 9 10 NaN 12];");
    eval("imagesc(C);");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    // row 1: v2, v3, v4
    EXPECT_TRUE(pixelMatches(this, "A", cellY(0), cellX(1), 71, 63, 227, 3));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(0), cellX(2), 70, 96, 251, 3));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(0), cellX(3), 47, 130, 250, 3));
    // row 2: v6, v7, v8
    EXPECT_TRUE(pixelMatches(this, "A", cellY(1), cellX(1), 4, 182, 206, 3));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(1), cellX(2), 47, 197, 163, 3));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(1), cellX(3), 103, 205, 108, 3));
    // row 3: v10, NaN, v12 — NaN renders as palette entry 1 (62,38,168),
    // the same color as the clim-saturated v1 (MATLAB behavior).
    EXPECT_TRUE(pixelMatches(this, "A", cellY(2), cellX(1), 245, 186, 59, 3));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(2), cellX(2), 62, 38, 168, 3));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(2), cellX(3), 249, 251, 21, 3));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(0), cellX(0), 62, 38, 168, 3));
    // Axes state round-trip (MATLAB probe A: YDir reverse, CLim [1 12]).
    EXPECT_EQ(evalString("get(gca, 'YDir');"), "reverse");
    auto cl = eval("get(gca, 'CLim');");
    EXPECT_NEAR(cl.numel(), 2, 0);
    removeFile(p);
}

// clim([2 11]) narrows the window: v1 saturates low, v10 high, v6 maps
// mid-window (MATLAB probe p35_c pixels).
TEST_P(FigureExportTest, ImagescClimOverride)
{
    std::string p = tmpPath("_nk_clim.png");
    eval("C = [1 2 3 4; 5 6 7 8; 9 10 NaN 12];");
    eval("imagesc(C); clim([2 11]);");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    EXPECT_TRUE(pixelMatches(this, "A", cellY(0), cellX(0), 62, 38, 168, 3));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(1), cellX(1), 8, 180, 209, 3));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(2), cellX(3), 249, 251, 21, 3));
    removeFile(p);
}

// colormap(M) custom palette: gray(4) endpoints are exact under any
// rounding rule (t=0 → entry 1, t=1 → entry 4).
TEST_P(FigureExportTest, ImagescCustomGrayPalette)
{
    std::string p = tmpPath("_nk_gray.png");
    eval("C = [1 2 3 4; 5 6 7 8; 9 10 NaN 12];");
    eval("imagesc(C); colormap(gray(4));");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    EXPECT_TRUE(pixelMatches(this, "A", cellY(0), cellX(0), 0, 0, 0, 0));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(2), cellX(2), 0, 0, 0, 0));
    EXPECT_TRUE(pixelMatches(this, "A", cellY(2), cellX(3), 255, 255, 255, 0));
    removeFile(p);
}

// imshow grayscale: uint8 values render as their own gray level
// (CLim [0 255]) with axis off and square pixels.
TEST_P(FigureExportTest, ImshowGrayscalePixelExact)
{
    std::string p = tmpPath("_nk_gray2.png");
    eval("imshow(uint8([0 64 128; 192 255 32]));");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    EXPECT_NEAR(evalScalar("size(A,1)"), 630.0, 0.5);
    EXPECT_NEAR(evalScalar("size(A,2)"), 840.0, 0.5);
    // Layout-independent counts: every gray level paints a solid block.
    for (int v : {0, 64, 128, 192, 255, 32}) {
        std::string vv = std::to_string(v);
        EXPECT_GT(evalScalar(
            "sum(sum(A(:,:,1) == " + vv + " & A(:,:,2) == " + vv
            + " & A(:,:,3) == " + vv + "))"), 500.0) << "gray " << v;
    }
    removeFile(p);
}

// imshow truecolor: RGB bytes pass through unmodified.
TEST_P(FigureExportTest, ImshowRgbPixelExact)
{
    std::string p = tmpPath("_nk_rgb.png");
    eval("I = zeros(2, 3, 3, 'uint8');");
    eval("I(1,1,:) = [255 0 0]; I(1,2,:) = [0 255 0]; I(1,3,:) = [0 0 255];");
    eval("I(2,1,:) = [255 255 0]; I(2,2,:) = [0 255 255]; I(2,3,:) = [255 0 255];");
    eval("imshow(I);");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    // Count exact-color blocks (each cell ≥ 3000 px).
    EXPECT_GT(evalScalar("sum(sum(A(:,:,1) == 255 & A(:,:,2) == 0 & A(:,:,3) == 0))"), 3000.0);
    EXPECT_GT(evalScalar("sum(sum(A(:,:,1) == 0 & A(:,:,2) == 255 & A(:,:,3) == 0))"), 3000.0);
    EXPECT_GT(evalScalar("sum(sum(A(:,:,1) == 0 & A(:,:,2) == 0 & A(:,:,3) == 255))"), 3000.0);
    EXPECT_GT(evalScalar("sum(sum(A(:,:,1) == 255 & A(:,:,2) == 255 & A(:,:,3) == 0))"), 3000.0);
    EXPECT_GT(evalScalar("sum(sum(A(:,:,1) == 0 & A(:,:,2) == 255 & A(:,:,3) == 255))"), 3000.0);
    EXPECT_GT(evalScalar("sum(sum(A(:,:,1) == 255 & A(:,:,2) == 0 & A(:,:,3) == 255))"), 3000.0);
    removeFile(p);
}

// SVG export embeds the image as a base64 PNG <image> element.
TEST_P(FigureExportTest, SvgEmbedsImagesc)
{
    std::string p = tmpPath("_nk_img.svg");
    eval("imagesc([1 2; 3 4]);");
    eval("saveas(gcf, '" + p + "', 'svg');");
    FILE *f = std::fopen(p.c_str(), "rb");
    ASSERT_NE(f, nullptr);
    std::string svg;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        svg.append(buf, n);
    std::fclose(f);
    EXPECT_NE(svg.find("<image"), std::string::npos);
    EXPECT_NE(svg.find("data:image/png;base64,"), std::string::npos);
    removeFile(p);
}
