// tests/graphics/figure_export_test.cpp
//
// saveas / exportgraphics / print — the engine-side figure export
// (portion 32). Dual-engine per the portion-close gate. Files are
// written through the engine fs into the temp area and checked for
// magic bytes / required content, then removed.

#include "dual_engine_fixture.hpp"

#include <cstdio>
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
