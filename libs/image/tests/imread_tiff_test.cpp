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

// ── compression decoders (cycle 92) ─────────────────────────────────

// PackBits: deterministic run-length-encoded uint8 gray must match
// the uncompressed reference byte-for-byte.
TEST_F(ImreadTiffTest, PackBitsMatchesUncompressed)
{
    engine.eval("A = imread('" + path("gray8_none.tif") + "');");
    engine.eval("B = imread('" + path("gray8_packbits.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("isequal(A, B)").toScalar(), 1.0);
}

// LZW with horizontal predictor (TIFF's default for imwrite).
TEST_F(ImreadTiffTest, LzwMatchesUncompressed)
{
    engine.eval("A = imread('" + path("gray8_none.tif") + "');");
    engine.eval("B = imread('" + path("gray8_lzw.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("isequal(A, B)").toScalar(), 1.0);
}

// RGB-8 with LZW exercises chunky sample interleaving.
TEST_F(ImreadTiffTest, LzwRgb8)
{
    engine.eval("A = imread('" + path("rgb8_none.tif") + "');");
    engine.eval("B = imread('" + path("rgb8_lzw.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("isequal(A, B)").toScalar(), 1.0);
}

// 16-bit gray with PackBits exercises byte-pair packing.
TEST_F(ImreadTiffTest, PackBitsGray16)
{
    engine.eval("A = imread('" + path("gray16_none.tif") + "');");
    engine.eval("B = imread('" + path("gray16_packbits.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("isequal(A, B)").toScalar(), 1.0);
    EXPECT_EQ(engine.eval("class(B)").toString(), "uint16");
}

// LZW + 16-bit gray exercises the multi-byte horizontal predictor.
TEST_F(ImreadTiffTest, LzwGray16)
{
    engine.eval("A = imread('" + path("gray16_none.tif") + "');");
    engine.eval("B = imread('" + path("gray16_lzw.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("isequal(A, B)").toScalar(), 1.0);
}

// Deflate (zlib) now works end-to-end on MATLAB-generated fixtures.
TEST_F(ImreadTiffTest, DeflateMatchesUncompressed)
{
    engine.eval("A = imread('" + path("gray8_none.tif") + "');");
    engine.eval("B = imread('" + path("gray8_deflate.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("isequal(A, B)").toScalar(), 1.0);
}

TEST_F(ImreadTiffTest, DeflateRgb8)
{
    engine.eval("A = imread('" + path("rgb8_none.tif") + "');");
    engine.eval("B = imread('" + path("rgb8_deflate.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("isequal(A, B)").toScalar(), 1.0);
}

TEST_F(ImreadTiffTest, DeflateGray16)
{
    engine.eval("A = imread('" + path("gray16_none.tif") + "');");
    engine.eval("B = imread('" + path("gray16_deflate.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("isequal(A, B)").toScalar(), 1.0);
}

// ── multi-page (cycle 92) ────────────────────────────────────────────

TEST_F(ImreadTiffTest, MultiPageReadsAllPages)
{
    engine.eval("p = '" + path("multipage_gray8.tif") + "';");
    engine.eval("A1 = imread(p, 1); A2 = imread(p, 2); A3 = imread(p, 3);");
    EXPECT_DOUBLE_EQ(engine.eval("A1(1,1)").toScalar(),   1.0);
    EXPECT_DOUBLE_EQ(engine.eval("A1(4,4)").toScalar(),  16.0);
    EXPECT_DOUBLE_EQ(engine.eval("A2(1,1)").toScalar(), 100.0);
    EXPECT_DOUBLE_EQ(engine.eval("A2(4,4)").toScalar(), 115.0);
    EXPECT_DOUBLE_EQ(engine.eval("A3(1,1)").toScalar(), 200.0);
    EXPECT_DOUBLE_EQ(engine.eval("A3(4,4)").toScalar(), 215.0);
}

TEST_F(ImreadTiffTest, MultiPageOutOfRangeThrows)
{
    EXPECT_THROW(engine.eval("imread('" + path("multipage_gray8.tif") + "', 4);"),
                 std::exception);
}

// ── palette photometric=3 (cycle 92) ─────────────────────────────────

TEST_F(ImreadTiffTest, PaletteReturnsIndexed)
{
    // Single-output `imread` of a palette TIFF returns the raw indices
    // (matches MATLAB's default; ind2rgb separately maps via cmap).
    engine.eval("A = imread('" + path("palette_indexed.tif") + "');");
    EXPECT_EQ(engine.eval("class(A)").toString(), "uint8");
    EXPECT_DOUBLE_EQ(engine.eval("size(A,1)").toScalar(), 4.0);
    EXPECT_DOUBLE_EQ(engine.eval("size(A,2)").toScalar(), 4.0);
}

// ── writer round-trip (cycle 92) ─────────────────────────────────────
//
// Writes a deterministic image through every compression scheme, reads
// it back, asserts isequal. Tempname keeps the test self-contained.

class TiffWriterTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
    void roundTrip(const std::string &mat, const std::string &comp) {
        engine.eval("p = [tempname '.tif']; "
                    + mat + " "
                    "imwrite(A, p, 'tif', 'Compression', '" + comp + "'); "
                    "B = imread(p); "
                    "ok = isequal(A, B); "
                    "delete(p);");
        EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0) << "scheme=" << comp;
    }
};

TEST_F(TiffWriterTest, Gray8RoundTripAllSchemes)
{
    const std::string mat =
        "A = uint8(reshape(mod((0:63)*17, 256), 8, 8));";
    for (auto c : {"none", "packbits", "lzw", "deflate"})
        roundTrip(mat, c);
}

TEST_F(TiffWriterTest, Rgb8RoundTripAllSchemes)
{
    const std::string mat = "A = uint8(reshape(0:191, 8, 8, 3));";
    for (auto c : {"none", "packbits", "lzw", "deflate"})
        roundTrip(mat, c);
}

TEST_F(TiffWriterTest, Uint16RoundTripAllSchemes)
{
    const std::string mat =
        "A = uint16(reshape(50000:50063, 8, 8));";
    for (auto c : {"none", "packbits", "lzw", "deflate"})
        roundTrip(mat, c);
}

TEST_F(TiffWriterTest, MultiPageWriteAndRead)
{
    engine.eval(
        "p = [tempname '.tif']; "
        "imwrite(uint8(reshape(1:16, 4, 4)),    p, 'tif', 'Compression', 'none'); "
        "imwrite(uint8(reshape(100:115, 4, 4)), p, 'tif', 'Compression', 'none', 'WriteMode', 'append'); "
        "imwrite(uint8(reshape(200:215, 4, 4)), p, 'tif', 'Compression', 'none', 'WriteMode', 'append'); "
        "A1 = imread(p, 1); A2 = imread(p, 2); A3 = imread(p, 3); "
        "delete(p);");
    EXPECT_DOUBLE_EQ(evalScalar("A1(1,1)"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("A1(4,4)"),  16.0);
    EXPECT_DOUBLE_EQ(evalScalar("A2(1,1)"), 100.0);
    EXPECT_DOUBLE_EQ(evalScalar("A3(4,4)"), 215.0);
}

TEST_F(TiffWriterTest, UnknownCompressionThrows)
{
    engine.eval("p = [tempname '.tif']; A = uint8([1 2; 3 4]);");
    EXPECT_THROW(engine.eval("imwrite(A, p, 'tif', 'Compression', 'bogus');"),
                 std::exception);
}

// ── gap-closure cycle: items 1-4 ─────────────────────────────────────

// Item 1: [A, map] = imread(palette_file) — two-output form returns
// the colormap as K×3 DOUBLE in [0, 1].
TEST_F(ImreadTiffTest, PaletteTwoOutputReturnsColormap)
{
    engine.eval("[A, map] = imread('" + path("palette_indexed.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("size(map,1)").toScalar(), 256.0);
    EXPECT_DOUBLE_EQ(engine.eval("size(map,2)").toScalar(),   3.0);
    EXPECT_EQ(engine.eval("class(map)").toString(), "double");
    // The 5-entry MATLAB cmap was [0 0 0; 1 0 0; 0 1 0; 0 0 1; 1 1 1].
    EXPECT_DOUBLE_EQ(engine.eval("map(2,1)").toScalar(), 1.0);  // R of red
    EXPECT_DOUBLE_EQ(engine.eval("map(2,2)").toScalar(), 0.0);  // G of red
    EXPECT_DOUBLE_EQ(engine.eval("map(3,2)").toScalar(), 1.0);  // G of green
    EXPECT_DOUBLE_EQ(engine.eval("map(4,3)").toScalar(), 1.0);  // B of blue
    EXPECT_DOUBLE_EQ(engine.eval("map(5,1)").toScalar(), 1.0);  // white
    EXPECT_DOUBLE_EQ(engine.eval("map(5,3)").toScalar(), 1.0);
}

// Item 2: BigTIFF reader — magic 43, 8-byte IFD offsets / counts.
TEST_F(ImreadTiffTest, BigTiffGray8)
{
    engine.eval("A = imread('" + path("bigtiff_gray8.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("size(A,1)").toScalar(), 4.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(1,1)").toScalar(),  1.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(4,4)").toScalar(), 16.0);
}

TEST_F(ImreadTiffTest, BigTiffRgb8)
{
    engine.eval("A = imread('" + path("bigtiff_rgb8.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("size(A,3)").toScalar(), 3.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(1,1,1)").toScalar(),  0.0);
    EXPECT_DOUBLE_EQ(engine.eval("A(4,4,3)").toScalar(), 47.0);
}

TEST_F(ImreadTiffTest, BigTiffImfinfo)
{
    engine.eval("s = imfinfo('" + path("bigtiff_gray8.tif") + "');");
    EXPECT_DOUBLE_EQ(engine.eval("s.Width").toScalar(),  4.0);
    EXPECT_DOUBLE_EQ(engine.eval("s.Height").toScalar(), 4.0);
}

// Item 3: Writer LZW with horizontal predictor — round-trip + read back.
TEST_F(TiffWriterTest, LzwWithPredictorRoundTrip)
{
    engine.eval("p = [tempname '.tif']; "
                "A = uint8(reshape(mod((0:63)*17, 256), 8, 8)); "
                "imwrite(A, p, 'tif', 'Compression', 'lzw'); "
                "B = imread(p); ok = isequal(A, B); delete(p);");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
}

// Item 4: Writer integer / float dtypes — round-trip preserves class.
TEST_F(TiffWriterTest, Int16RoundTrip)
{
    engine.eval("p = [tempname '.tif']; "
                "A = int16([-1000 0 1000; 2000 -2000 32000]); "
                "imwrite(A, p, 'tif'); B = imread(p); "
                "ok = isequal(A, B); cls = class(B); delete(p);");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
    EXPECT_EQ(engine.eval("cls").toString(), "int16");
}

TEST_F(TiffWriterTest, Int32RoundTrip)
{
    engine.eval("p = [tempname '.tif']; "
                "A = int32([-100000 200000; 300000 -400000]); "
                "imwrite(A, p, 'tif'); B = imread(p); "
                "ok = isequal(A, B); cls = class(B); delete(p);");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
    EXPECT_EQ(engine.eval("cls").toString(), "int32");
}

TEST_F(TiffWriterTest, SingleRoundTrip)
{
    engine.eval("p = [tempname '.tif']; "
                "A = single([1.5 2.5 3.5; -1.5 -2.5 -3.5]); "
                "imwrite(A, p, 'tif'); B = imread(p); "
                "ok = isequal(A, B); cls = class(B); delete(p);");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
    EXPECT_EQ(engine.eval("cls").toString(), "single");
}

TEST_F(TiffWriterTest, DoubleRoundTrip)
{
    engine.eval("p = [tempname '.tif']; "
                "A = [1.234567890123 -3.14159265359; 2.718281828 0.0]; "
                "imwrite(A, p, 'tif'); B = imread(p); "
                "ok = isequal(A, B); cls = class(B); delete(p);");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
    EXPECT_EQ(engine.eval("cls").toString(), "double");
}
