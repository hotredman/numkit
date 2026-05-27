// libs/image/tests/imread_tiff_test.cpp
//
// Regression guard for the minimal TIFF reader (cycle 90 baseline).
// Fixtures are written by MATLAB R2025b imwrite (uncompressed baseline)
// and live in fixtures/.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <string>

class ImreadTiffTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    std::filesystem::path fixtures;

    void SetUp() override
    {
        engine.eval("import compat.*;");
        // __FILE__ points at this source file at compile time; fixtures/
        // is a sibling directory. Robust across desktop and CI builds.
        fixtures = std::filesystem::path(__FILE__).parent_path() / "fixtures";
    }
    std::string path(const std::string &name) const
    {
        return (fixtures / name).string();
    }
};

// 4x4 uint8 grayscale TIFF.
TEST_F(ImreadTiffTest, Gray8)
{
    engine.eval("A = imread('" + path("gray8.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("size(A,1)").toScalar(), 4.0);
    EXPECT_DOUBLE_EQ(engine.eval("size(A,2)").toScalar(), 4.0);
    EXPECT_EQ(engine.eval("class(A)").toString(), "uint8");
    // reshape(1:16, 4, 4) → column-major matrix; A(r,c) below is 1-based.
    EXPECT_DOUBLE_EQ(engine.eval("A(1,1)").toScalar(),  1.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(2,1)").toScalar(),  2.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(1,2)").toScalar(),  5.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(4,4)").toScalar(), 16.0);
}

// 4x4x3 uint8 RGB TIFF.
TEST_F(ImreadTiffTest, Rgb8)
{
    engine.eval("A = imread('" + path("rgb8.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("size(A,1)").toScalar(), 4.0);
    EXPECT_DOUBLE_EQ(engine.eval("size(A,2)").toScalar(), 4.0);
    EXPECT_DOUBLE_EQ(engine.eval("size(A,3)").toScalar(), 3.0);
    EXPECT_EQ(engine.eval("class(A)").toString(), "uint8");
    // reshape(0:47, 4, 4, 3): channel 1 = 0..15, channel 2 = 16..31, channel 3 = 32..47.
    EXPECT_DOUBLE_EQ(engine.eval("A(1,1,1)").toScalar(),  0.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(4,4,1)").toScalar(), 15.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(1,1,2)").toScalar(), 16.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(4,4,3)").toScalar(), 47.0);
}

// 4x4 uint16 grayscale TIFF.
TEST_F(ImreadTiffTest, Gray16)
{
    engine.eval("A = imread('" + path("gray16.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("size(A,1)").toScalar(), 4.0);
    EXPECT_DOUBLE_EQ(engine.eval("size(A,2)").toScalar(), 4.0);
    EXPECT_EQ(engine.eval("class(A)").toString(), "uint16");
    // reshape(1000:1015, 4, 4) column-major.
    EXPECT_DOUBLE_EQ(engine.eval("A(1,1)").toScalar(), 1000.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(4,4)").toScalar(), 1015.0);
}

// imfinfo on TIFF returns Width/Height/Channels/Format.
TEST_F(ImreadTiffTest, ImfinfoRgb8)
{
    engine.eval("s = imfinfo('" + path("rgb8.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("s.Width").toScalar(),  4.0);
    EXPECT_DOUBLE_EQ(engine.eval("s.Height").toScalar(), 4.0);
    EXPECT_DOUBLE_EQ(engine.eval("s.NumberOfChannels").toScalar(), 3.0);
    EXPECT_EQ(engine.eval("s.Format").toString(), "tif");
}

TEST_F(ImreadTiffTest, ImfinfoGray16)
{
    engine.eval("s = imfinfo('" + path("gray16.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("s.Width").toScalar(),  4.0);
    EXPECT_DOUBLE_EQ(engine.eval("s.Height").toScalar(), 4.0);
    EXPECT_DOUBLE_EQ(engine.eval("s.NumberOfChannels").toScalar(), 1.0);
    EXPECT_DOUBLE_EQ(engine.eval("s.BitDepth").toScalar(), 16.0);
}
