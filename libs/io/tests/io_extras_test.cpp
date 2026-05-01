// libs/io/tests/io_extras_test.cpp
//
// Tests for C1 (text helpers: fileread/readlines/writelines/
//                              readmatrix/writematrix/type) and
// C2 (paths: filesep/fullfile/fileparts/tempdir/tempname).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace numkit;

class IoExtrasTest : public ::testing::Test
{
public:
    Engine engine;
    std::string capturedOut;
    std::filesystem::path tmpFile;

    void SetUp() override
    {
        engine.setOutputFunc([this](const std::string &s) { capturedOut += s; });
        engine.eval("import compat.*;");
        // Per-test temp file — host filesystem (resolvePath → native).
        auto td = std::filesystem::temp_directory_path();
        tmpFile = td / ("numkit_io_extras_" +
                         std::to_string(std::hash<std::string>()(::testing::UnitTest::GetInstance()
                             ->current_test_info()->name())) + ".txt");
        if (std::filesystem::exists(tmpFile))
            std::filesystem::remove(tmpFile);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(tmpFile, ec);
    }

    Value eval(const std::string &c) { return engine.eval(c); }
    std::string evalString(const std::string &c) { return eval(c).toString(); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }

    void writeTempFile(const std::string &content)
    {
        std::ofstream f(tmpFile, std::ios::binary);
        f << content;
    }

    std::string readTempFile()
    {
        std::ifstream f(tmpFile, std::ios::binary);
        std::ostringstream os; os << f.rdbuf();
        return os.str();
    }

    std::string tmpPathForMatlab()
    {
        // Forward slashes work on Windows too in std::filesystem and our VFS.
        std::string s = tmpFile.string();
        for (auto &c : s) if (c == '\\') c = '/';
        return s;
    }
};

// ── C1 — text helpers ────────────────────────────────────────────────

TEST_F(IoExtrasTest, FilereadRoundtrip)
{
    writeTempFile("hello\nworld");
    auto v = eval("fileread('" + tmpPathForMatlab() + "');");
    EXPECT_EQ(v.toString(), "hello\nworld");
}

TEST_F(IoExtrasTest, ReadlinesSplitsOnLf)
{
    writeTempFile("alpha\nbeta\ngamma\n");
    auto v = eval("readlines('" + tmpPathForMatlab() + "');");
    ASSERT_EQ(v.numel(), 3u);
    EXPECT_EQ(v.stringElem(0), "alpha");
    EXPECT_EQ(v.stringElem(1), "beta");
    EXPECT_EQ(v.stringElem(2), "gamma");
}

TEST_F(IoExtrasTest, ReadlinesHandlesCrLf)
{
    writeTempFile("line1\r\nline2\r\n");
    auto v = eval("readlines('" + tmpPathForMatlab() + "');");
    ASSERT_EQ(v.numel(), 2u);
    EXPECT_EQ(v.stringElem(0), "line1");
    EXPECT_EQ(v.stringElem(1), "line2");
}

TEST_F(IoExtrasTest, WritelinesCellRoundtrip)
{
    eval("writelines({'one', 'two', 'three'}, '" + tmpPathForMatlab() + "');");
    auto content = readTempFile();
    // Content should contain the three words separated by line endings.
    EXPECT_NE(content.find("one"),   std::string::npos);
    EXPECT_NE(content.find("two"),   std::string::npos);
    EXPECT_NE(content.find("three"), std::string::npos);

    // Round-trip via readlines.
    auto v = eval("readlines('" + tmpPathForMatlab() + "');");
    ASSERT_EQ(v.numel(), 3u);
    EXPECT_EQ(v.stringElem(0), "one");
}

TEST_F(IoExtrasTest, ReadmatrixSimple)
{
    writeTempFile("1,2,3\n4,5,6\n");
    auto v = eval("M = readmatrix('" + tmpPathForMatlab() + "');");
    EXPECT_EQ(v.dims().rows(), 2u);
    EXPECT_EQ(v.dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(v(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(v(1, 2), 6.0);
}

TEST_F(IoExtrasTest, ReadmatrixSkipsHeaderRow)
{
    writeTempFile("col1,col2\n10,20\n30,40\n");
    auto v = eval("M = readmatrix('" + tmpPathForMatlab() + "');");
    EXPECT_EQ(v.dims().rows(), 2u);
    EXPECT_EQ(v.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(v(0, 0), 10.0);
    EXPECT_DOUBLE_EQ(v(1, 1), 40.0);
}

TEST_F(IoExtrasTest, WritematrixRoundtrip)
{
    eval("M = [1 2 3; 4 5 6];");
    eval("writematrix(M, '" + tmpPathForMatlab() + "');");
    auto v = eval("M2 = readmatrix('" + tmpPathForMatlab() + "');");
    EXPECT_EQ(v.dims().rows(), 2u);
    EXPECT_EQ(v.dims().cols(), 3u);
    EXPECT_DOUBLE_EQ(v(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(v(1, 2), 6.0);
}

TEST_F(IoExtrasTest, TypePrintsContent)
{
    writeTempFile("file content here");
    capturedOut.clear();
    eval("type('" + tmpPathForMatlab() + "');");
    EXPECT_NE(capturedOut.find("file content here"), std::string::npos);
}

// ── C2 — paths (fully-qualified io.paths.* — short names hit older
//     duplicates in libs/builtin until Session A removes them) ──────

TEST_F(IoExtrasTest, FilesepReturnsSingleChar)
{
    auto v = eval("io.paths.filesep();");
    EXPECT_EQ(v.numel(), 1u);
    const char c = v.charElem(0);
    EXPECT_TRUE(c == '/' || c == '\\');
}

TEST_F(IoExtrasTest, FullfileJoinsParts)
{
    auto v = eval("io.paths.fullfile('foo', 'bar', 'baz.txt');");
    auto s = v.toString();
    EXPECT_NE(s.find("foo"), std::string::npos);
    EXPECT_NE(s.find("bar"), std::string::npos);
    EXPECT_NE(s.find("baz.txt"), std::string::npos);
}

TEST_F(IoExtrasTest, FullfileStripsRedundantSeparators)
{
    auto v = eval("io.paths.fullfile('foo/', '/bar/', 'baz');");
    auto s = v.toString();
    EXPECT_EQ(s.find("//"), std::string::npos);
    EXPECT_EQ(s.find("\\\\"), std::string::npos);
}

TEST_F(IoExtrasTest, FilepartsThreeOutputs)
{
    eval("[d, n, e] = io.paths.fileparts('/usr/local/bin/script.m');");
    EXPECT_EQ(evalString("d"), "/usr/local/bin");
    EXPECT_EQ(evalString("n"), "script");
    EXPECT_EQ(evalString("e"), ".m");
}

TEST_F(IoExtrasTest, FilepartsNoExtension)
{
    eval("[d, n, e] = io.paths.fileparts('script');");
    EXPECT_EQ(evalString("d"), "");
    EXPECT_EQ(evalString("n"), "script");
    EXPECT_EQ(evalString("e"), "");
}

TEST_F(IoExtrasTest, FilepartsHiddenDotfile)
{
    eval("[d, n, e] = io.paths.fileparts('.bashrc');");
    EXPECT_EQ(evalString("n"), ".bashrc");
    EXPECT_EQ(evalString("e"), "");
}

TEST_F(IoExtrasTest, TempdirNonEmpty)
{
    auto v = eval("io.paths.tempdir();");
    EXPECT_GT(v.numel(), 0u);
}

TEST_F(IoExtrasTest, TempnameIsUnique)
{
    auto a = evalString("io.paths.tempname();");
    auto b = evalString("io.paths.tempname();");
    EXPECT_NE(a, b);
}

TEST_F(IoExtrasTest, TempnameInTempdir)
{
    auto td = evalString("io.paths.tempdir();");
    auto tn = evalString("io.paths.tempname();");
    EXPECT_EQ(tn.compare(0, td.size(), td), 0);
}
