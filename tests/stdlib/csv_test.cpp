// tests/stdlib/csv_test.cpp — csvread / csvwrite MATLAB parity
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace mlab_test;

class CsvTest : public DualEngineTest
{
public:
    std::filesystem::path tmpPath;

    void SetUp() override
    {
        DualEngineTest::SetUp();
        auto base = std::filesystem::temp_directory_path();
        static std::atomic<unsigned> counter{0};
        unsigned id = counter.fetch_add(1);
        tmpPath = base / (std::string("mlab_csv_test_") + std::to_string(id) + ".csv");
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(tmpPath, ec);
        DualEngineTest::TearDown();
    }

    std::string path() const { return tmpPath.string(); }

    void writeRaw(const std::string &content)
    {
        std::ofstream f(tmpPath, std::ios::binary);
        f << content;
    }

    std::string readRaw() const
    {
        std::ifstream f(tmpPath, std::ios::binary);
        std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return s;
    }
};

TEST_P(CsvTest, WriteThenReadRoundTrip)
{
    eval("A = [1 2 3; 4 5 6; 7 8 9];");
    eval("csvwrite('" + path() + "', A);");
    eval("B = csvread('" + path() + "');");

    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(rows(*B), 3u);
    EXPECT_EQ(cols(*B), 3u);
    for (size_t r = 0; r < 3; ++r)
        for (size_t c = 0; c < 3; ++c)
            expectElem2D(*B, r, c, static_cast<double>(r * 3 + c + 1));
}

TEST_P(CsvTest, WriteProducesCommaSeparatedLines)
{
    eval("csvwrite('" + path() + "', [1 2; 3 4]);");
    std::string contents = readRaw();
    EXPECT_EQ(contents, "1,2\n3,4\n");
}

TEST_P(CsvTest, ReadSkipsNoExtensionAddsCsv)
{
    writeRaw("10,20\n30,40\n");
    // Use the path without extension — csvread should append ".csv"
    std::string noExt = path();
    noExt = noExt.substr(0, noExt.size() - 4); // strip ".csv"
    eval("M = csvread('" + noExt + "');");

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(rows(*M), 2u);
    EXPECT_EQ(cols(*M), 2u);
    expectElem2D(*M, 0, 0, 10.0);
    expectElem2D(*M, 0, 1, 20.0);
    expectElem2D(*M, 1, 0, 30.0);
    expectElem2D(*M, 1, 1, 40.0);
}

TEST_P(CsvTest, ReadWithRowColOffset)
{
    writeRaw("1,2,3\n4,5,6\n7,8,9\n");
    eval("M = csvread('" + path() + "', 1, 1);");

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(rows(*M), 2u);
    EXPECT_EQ(cols(*M), 2u);
    expectElem2D(*M, 0, 0, 5.0);
    expectElem2D(*M, 0, 1, 6.0);
    expectElem2D(*M, 1, 0, 8.0);
    expectElem2D(*M, 1, 1, 9.0);
}

TEST_P(CsvTest, ReadWithExplicitRange)
{
    writeRaw("1,2,3,4\n5,6,7,8\n9,10,11,12\n13,14,15,16\n");
    eval("M = csvread('" + path() + "', 1, 1, [1 1 2 2]);");

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(rows(*M), 2u);
    EXPECT_EQ(cols(*M), 2u);
    expectElem2D(*M, 0, 0, 6.0);
    expectElem2D(*M, 0, 1, 7.0);
    expectElem2D(*M, 1, 0, 10.0);
    expectElem2D(*M, 1, 1, 11.0);
}

TEST_P(CsvTest, ReadTreatsMissingCellsAsZero)
{
    writeRaw("1,,3\n,5,\n");
    eval("M = csvread('" + path() + "');");

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(rows(*M), 2u);
    EXPECT_EQ(cols(*M), 3u);
    expectElem2D(*M, 0, 0, 1.0);
    expectElem2D(*M, 0, 1, 0.0);
    expectElem2D(*M, 0, 2, 3.0);
    expectElem2D(*M, 1, 0, 0.0);
    expectElem2D(*M, 1, 1, 5.0);
    expectElem2D(*M, 1, 2, 0.0);
}

TEST_P(CsvTest, ReadPadsShortRowsWithZeros)
{
    writeRaw("1,2,3\n4,5\n6\n");
    eval("M = csvread('" + path() + "');");

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(rows(*M), 3u);
    EXPECT_EQ(cols(*M), 3u);
    expectElem2D(*M, 0, 0, 1.0);
    expectElem2D(*M, 0, 2, 3.0);
    expectElem2D(*M, 1, 0, 4.0);
    expectElem2D(*M, 1, 2, 0.0);
    expectElem2D(*M, 2, 0, 6.0);
    expectElem2D(*M, 2, 1, 0.0);
    expectElem2D(*M, 2, 2, 0.0);
}

TEST_P(CsvTest, WriteWithOffsetProducesLeadingBlanks)
{
    eval("csvwrite('" + path() + "', [7 8; 9 10], 1, 2);");
    std::string contents = readRaw();
    EXPECT_EQ(contents, "\n,,7,8\n,,9,10\n");
}

TEST_P(CsvTest, WriteHandlesScientificAndNegative)
{
    eval("A = [-1.5 2.5; 0 1e-3];");
    eval("csvwrite('" + path() + "', A);");
    eval("B = csvread('" + path() + "');");

    auto *B = getVarPtr("B");
    ASSERT_NE(B, nullptr);
    EXPECT_EQ(rows(*B), 2u);
    EXPECT_EQ(cols(*B), 2u);
    expectElem2D(*B, 0, 0, -1.5);
    expectElem2D(*B, 0, 1, 2.5);
    expectElem2D(*B, 1, 0, 0.0);
    expectElem2D(*B, 1, 1, 1e-3);
}

TEST_P(CsvTest, ReadToleratesCrLfLineEndings)
{
    writeRaw("1,2,3\r\n4,5,6\r\n");
    eval("M = csvread('" + path() + "');");

    auto *M = getVarPtr("M");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(rows(*M), 2u);
    EXPECT_EQ(cols(*M), 3u);
    expectElem2D(*M, 0, 2, 3.0);
    expectElem2D(*M, 1, 2, 6.0);
}

INSTANTIATE_DUAL(CsvTest);
