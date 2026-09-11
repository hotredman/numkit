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
