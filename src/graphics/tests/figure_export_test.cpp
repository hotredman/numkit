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

// ── colorbar + text() export (portion 38; MATLAB R2025b probes) ─────
namespace {
// Count pixels within ±tol of an RGB across the whole image.
double colorCount(FigureExportTest *t, const std::string &a, int r, int g,
                  int b, int tol)
{
    return t->evalScalar(
        "sum(sum(abs(double(A(:,:,1)) - " + std::to_string(r)
        + ") <= " + std::to_string(tol)
        + " & abs(double(A(:,:,2)) - " + std::to_string(g)
        + ") <= " + std::to_string(tol)
        + " & abs(double(A(:,:,3)) - " + std::to_string(b)
        + ") <= " + std::to_string(tol) + "))");
}
} // namespace

// East colorbar: parula gradient strip with the low end at the BOTTOM.
// Strip transect anchors are parula table entries round(t*255).
TEST_P(FigureExportTest, ColorbarEastGradient)
{
    std::string p = tmpPath("_nk_cb.png");
    eval("imagesc([1 2 3 4; 5 6 7 8; 9 10 NaN 12]); colorbar;");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    // Bottom of the strip ≈ parula entry 5 (64,43,183) — the first
    // entries are nearly identical, so ±tol covers entry 0..8.
    EXPECT_GT(colorCount(this, "A", 64, 43, 183, 6), 20.0);
    // Top ≈ parula entry 250+ (247,244,28).
    EXPECT_GT(colorCount(this, "A", 247, 244, 28, 8), 20.0);
    // Middle region ≈ entry 128 (16,190,186)-ish.
    EXPECT_GT(colorCount(this, "A", 16, 190, 186, 10), 20.0);
    removeFile(p);
}

// South colorbar with a custom gray(256) palette: horizontal gradient
// black→white left→right, plus the positional-location fix (the old
// parser dropped colorbar('southoutside') and kept 'east').
TEST_P(FigureExportTest, ColorbarSouthPositionalAndGray)
{
    std::string p = tmpPath("_nk_cbs.png");
    eval("imagesc([1 2 3 4; 5 6 7 8; 9 10 NaN 12]);");
    eval("colorbar('southoutside'); colormap(gray(256));");
    // Engine state: the positional location must land in the axes.
    EXPECT_EQ(engine.figureManager().currentAxes().colorbarLocation,
              "southoutside");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    EXPECT_GT(colorCount(this, "A", 4, 4, 4, 4), 20.0);      // left end
    EXPECT_GT(colorCount(this, "A", 254, 254, 254, 2), 20.0); // right end
    removeFile(p);
}

// clim override narrows the colorbar window: v1 saturates LOW (entry 1)
// and the strip's bottom stays entry 1 — same color as the NaN cell.
TEST_P(FigureExportTest, ColorbarClimOverride)
{
    std::string p = tmpPath("_nk_cbc.png");
    eval("imagesc([1 2 3 4; 5 6 7 8; 9 10 NaN 12]); colorbar;");
    eval("clim([2 11]);");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    EXPECT_GT(colorCount(this, "A", 62, 38, 168, 4), 100.0);
    removeFile(p);
}

// text() annotations: default black 10pt at data coords, colour specs
// (name letter, RGB triplet — the triplet used to THROW "Not a char
// array"), and HorizontalAlignment.
TEST_P(FigureExportTest, TextAnnotationsRender)
{
    std::string p = tmpPath("_nk_txt.png");
    eval("plot(1:10);");
    eval("text(5, 5, 'mid');");
    eval("text(2, 8, 'L', 'Color', [1 0.5 0], 'FontSize', 16);");
    eval("text(8, 3, 'R', 'HorizontalAlignment', 'right', 'Color', 'r');");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    // Orange triplet text (255,128,0) — the form that used to throw.
    EXPECT_GT(colorCount(this, "A", 255, 128, 0, 10), 20.0);
    // Red short-spec text: MATLAB-exact (255,0,0); a single 7-px glyph
    // at scale 1 is ~18 pixels, so the floor is low.
    EXPECT_GT(colorCount(this, "A", 255, 0, 0, 6), 10.0);
    // Default black 'mid' near data (5,5): deterministic layout —
    // xlo=0.55..10.45 → X(5)≈412, Y(5)≈329; scan a 40×24 box.
    double n = evalScalar(
        "sum(sum(A(318:338, 395:430, 1) < 100 & A(318:338, 395:430, 2) < 100"
        " & A(318:338, 395:430, 3) < 100))");
    EXPECT_GT(n, 15.0);
    removeFile(p);
}

// ── contour / contourf / histogram export (portion 39) ──────────────

// MATLAB probe: histogram([1 1 2 2 2 3 5 5 9]) → integer-rule bins,
// Values [2 3 1 0 2 0 0 0 1]. The counts vector is checked on the
// engine dataset directly (data-level anchor, layout-independent).
TEST_P(FigureExportTest, HistogramAutoBinsIntegerRule)
{
    eval("histogram([1 1 2 2 2 3 5 5 9]);");
    const auto &ds = engine.figureManager().currentAxes().datasets;
    ASSERT_FALSE(ds.empty());
    auto counts = figdetail::parseFlatDoubles(ds.back().yJson);
    const double exp[] = {2, 3, 1, 0, 2, 0, 0, 0, 1};
    ASSERT_EQ(counts.size(), 9u);
    for (int k = 0; k < 9; ++k)
        EXPECT_NEAR(counts[k], exp[k], 1e-12);
}

// MATLAB default FaceColor 'auto' = (0,0.447,0.741) with FaceAlpha
// 0.6 (ALWAYS — probe R2025b; not a theme effect): over the white
// canvas the bars composite to 0.6·(0,114,189)+0.4·white =
// (102,170,215). EdgeColor (0.851)x3 = (217,217,217); bars TOUCH.
TEST_P(FigureExportTest, HistogramDefaultColorsAndBars)
{
    std::string p = tmpPath("_nk_hist.png");
    eval("histogram([1 1 2 2 2 3 5 5 9]);");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    EXPECT_GT(colorCount(this, "A", 102, 170, 215, 6), 5000.0);
    EXPECT_GT(colorCount(this, "A", 217, 217, 217, 6), 10.0);
    removeFile(p);
}

// FaceAlpha=1 + pinned FaceColor = the deterministic exact-colour
// recipe (what parity specs pin): solid (0,114,189) bars.
TEST_P(FigureExportTest, HistogramOpaquePinnedColor)
{
    std::string p = tmpPath("_nk_hist2.png");
    eval("histogram([1 1 2 2 2 3 5 5 9], 'FaceColor', [0 0.447 0.741], 'FaceAlpha', 1);");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    EXPECT_GT(colorCount(this, "A", 0, 114, 189, 6), 5000.0);
    removeFile(p);
}

// Scott auto-binning for continuous data (probe: rand(1,100)-class
// widths floor to one significant digit; here width-0.5 classes).
TEST_P(FigureExportTest, HistogramExplicitBins)
{
    eval("histogram([0.2 0.3 0.4 0.4 0.6 0.9 1.1 1.1], [0 0.5 1.0 1.5]);");
    const auto &ds = engine.figureManager().currentAxes().datasets;
    ASSERT_FALSE(ds.empty());
    auto counts = figdetail::parseFlatDoubles(ds.back().yJson);
    ASSERT_EQ(counts.size(), 3u);
    // [0,0.5): 0.2 0.3 0.4 0.4 · [0.5,1.0): 0.6 0.9 · [1.0,1.5]: 1.1 1.1
    // (right edge inclusive only on the LAST bin).
    EXPECT_NEAR(counts[0], 4.0, 1e-12);
    EXPECT_NEAR(counts[1], 2.0, 1e-12);
    EXPECT_NEAR(counts[2], 2.0, 1e-12);
}

// contour(): null separators must SPLIT polylines (the old path joined
// unrelated segments); lines carry per-level colormap colours.
TEST_P(FigureExportTest, ContourLinesRender)
{
    std::string p = tmpPath("_nk_ct.png");
    eval("contour(peaks(10));");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    // Blue-dominant pixels (parula low levels present in peaks).
    double blue = evalScalar(
        "sum(sum(double(A(:,:,3)) > double(A(:,:,1)) + 30))");
    EXPECT_GT(blue, 300.0);
    removeFile(p);
}

// contourf(): filled bands (polygon datasets with null-separated
// rings) — large coloured coverage, classic peaks signature.
TEST_P(FigureExportTest, ContourfBandsRender)
{
    std::string p = tmpPath("_nk_ctf.png");
    eval("contourf(peaks(10));");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    double colored = evalScalar(
        "sum(sum(abs(double(A(:,:,1)) - double(A(:,:,2))) > 25"
        " | abs(double(A(:,:,2)) - double(A(:,:,3))) > 25))");
    EXPECT_GT(colored, 5000.0);
    removeFile(p);
}

// ── 3-D surf / mesh / waterfall export (portion 40) ─────────────────

// Orientation contract (probe-pinned, MATLAB R2025b): at view(0,90) a
// hot corner Z(1,1)=1 renders LEFT-BOTTOM of the surface area. This
// caught the wrong sy sign (xy component) during development.
TEST_P(FigureExportTest, SurfTopViewCornerOrientation)
{
    std::string p = tmpPath("_nk_3d.png");
    eval("Z = zeros(5); Z(1,1) = 1;");
    eval("surf(Z); view(0,90);");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    // Yellow (parula top) in the LEFT-BOTTOM quadrant of the canvas.
    EXPECT_GT(colorCount(this, "A", 249, 251, 21, 12), 200.0);
    double yellowLB = evalScalar(
        "sum(sum(A(300:620, 1:400, 1) > 200 & A(300:620, 1:400, 2) > 200"
        " & A(300:620, 1:400, 3) < 90))");
    double yellowTR = evalScalar(
        "sum(sum(A(1:300, 500:840, 1) > 200 & A(1:300, 500:840, 2) > 200"
        " & A(1:300, 500:840, 3) < 90))");
    EXPECT_GT(yellowLB, 100.0);
    EXPECT_NEAR(yellowTR, 0.0, 0.0);
    removeFile(p);
}

// Default view (-37.5, 30): full parula gradient on the surface.
TEST_P(FigureExportTest, SurfDefaultViewGradient)
{
    std::string p = tmpPath("_nk_3dd.png");
    eval("surf(peaks(20));");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    double blue = evalScalar(
        "sum(sum(double(A(:,:,3)) > 150 & double(A(:,:,1)) < 100))");
    double yellow = evalScalar(
        "sum(sum(double(A(:,:,1)) > 200 & double(A(:,:,2)) > 200"
        " & double(A(:,:,3)) < 90))");
    EXPECT_GT(blue, 1000.0);
    EXPECT_GT(yellow, 200.0);
    removeFile(p);
}

// mesh(): lattice of SEGMENTS coloured by Z (EdgeColor 'flat') — no
// filled faces: coloured coverage far below the surf fill.
TEST_P(FigureExportTest, MeshColoredByZ)
{
    std::string p = tmpPath("_nk_3dm.png");
    eval("mesh(peaks(20));");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    double chromatic = evalScalar(
        "sum(sum(abs(double(A(:,:,1)) - double(A(:,:,2))) > 20"
        " | abs(double(A(:,:,2)) - double(A(:,:,3))) > 20))");
    double blue = evalScalar(
        "sum(sum(double(A(:,:,3)) > 150 & double(A(:,:,1)) < 100))");
    double yellow = evalScalar(
        "sum(sum(double(A(:,:,1)) > 200 & double(A(:,:,2)) > 200"
        " & double(A(:,:,3)) < 90))");
    // Wireframe: present but sparse (< 1/4 of the surf fill density).
    EXPECT_GT(chromatic, 500.0);
    EXPECT_LT(chromatic, 40000.0);
    EXPECT_GT(blue, 100.0);
    EXPECT_GT(yellow, 30.0);
    removeFile(p);
}

// waterfall(): row polylines coloured by Z.
TEST_P(FigureExportTest, WaterfallRowsRender)
{
    std::string p = tmpPath("_nk_3dw.png");
    eval("waterfall(peaks(15));");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    double chromatic = evalScalar(
        "sum(sum(abs(double(A(:,:,1)) - double(A(:,:,2))) > 20"
        " | abs(double(A(:,:,2)) - double(A(:,:,3))) > 20))");
    EXPECT_GT(chromatic, 300.0);
    removeFile(p);
}

// ── findobj / quiver / annotation (portion 41) ───────────────────────

// findobj semantics (MATLAB R2025b probes): root scope, axes scope,
// chart-handle self-scope, Type / Color / -property filters. The
// linespec 'r' reads back MATLAB-exact [1 0 0] (explicit call intent,
// unlike the soft renderer palette).
TEST_P(FigureExportTest, FindobjScopesAndFilters)
{
    eval("figure; h1 = plot(1:10); hold on;");
    eval("h2 = plot(1:10, 'r'); t = title('T');");
    EXPECT_NEAR(evalScalar("numel(findobj('Type','line'))"), 2.0, 0);
    EXPECT_NEAR(evalScalar("numel(findobj(gca,'Type','line'))"), 2.0, 0);
    // MATLAB hides title/label texts from findobj (probe: 0 next to
    // a title); user text() stays findable.
    EXPECT_NEAR(evalScalar("numel(findobj('Type','text'))"), 0.0, 0);
    EXPECT_NEAR(evalScalar("numel(findobj(h2,'Type','line'))"), 1.0, 0);
    EXPECT_NEAR(evalScalar("numel(findobj(h2))"), 1.0, 0);
    EXPECT_NEAR(evalScalar("numel(findobj('Color','r'))"), 1.0, 0);
    // Same hiding: no user text() objects, so 0.
    EXPECT_NEAR(evalScalar("numel(findobj('-property','String'))"), 0.0, 0);
    auto c = eval("get(h2, 'Color');");
    EXPECT_NEAR(c.elemAsDouble(0), 1.0, 1e-12);
    EXPECT_NEAR(c.elemAsDouble(1), 0.0, 1e-12);
    EXPECT_NEAR(c.elemAsDouble(2), 0.0, 1e-12);
}

// quiver(): arrows render — shaft + chevron heads inside the plot.
TEST_P(FigureExportTest, QuiverArrowsRender)
{
    std::string p = tmpPath("_nk_qv.png");
    eval("quiver([1 2 3], [1 1 1], [0.5 0 -0.5], [0 0.5 0]);");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    // Content away from the frame: the middle band of the plot area.
    double n = evalScalar(
        "sum(sum(A(200:430, 200:640, 1) < 200))");
    EXPECT_GT(n, 30.0);
    removeFile(p);
}

// annotation(): figure-normalized overlays — yellow ellipse face, red
// textarrow text, gray arrow shaft (MATLAB defaults (0.1294)^3).
TEST_P(FigureExportTest, AnnotationOverlaysRender)
{
    std::string p = tmpPath("_nk_an.png");
    eval("plot(1:10);");
    eval("annotation('arrow',[0.2 0.8],[0.2 0.8]);");
    eval("annotation('textarrow',[0.15 0.7],[0.8 0.6],'String','note','Color','r');");
    eval("annotation('ellipse',[0.55 0.1 0.15 0.15],'FaceColor',[1 1 0]);");
    eval("saveas(gcf, '" + p + "');");
    eval("A = imread('" + p + "');");
    EXPECT_GT(colorCount(this, "A", 255, 255, 0, 8), 200.0);   // ellipse
    EXPECT_GT(colorCount(this, "A", 255, 0, 0, 10), 5.0);      // red text
    double arrow = evalScalar(
        "sum(sum(A(150:300, 150:700, 1) > 20 & A(150:300, 150:700, 1) < 60"
        " & abs(double(A(150:300,150:700,1)) - double(A(150:300,150:700,2))) < 12"
        " & abs(double(A(150:300,150:700,2)) - double(A(150:300,150:700,3))) < 12))");
    EXPECT_GT(arrow, 30.0);
    removeFile(p);
}
