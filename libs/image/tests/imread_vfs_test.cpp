// libs/image/tests/imread_vfs_test.cpp
//
// Regression guard for the binary-safe VFS read path that imread uses in
// the IDE (WASM) build. Two invariants:
//
//   1. imread pulls file content through the engine's VFS prosloyka via
//      readFileBytes() — never a direct fopen. So it works on a virtual
//      filesystem (IndexedDB / Local Folder bridge) as well as on disk.
//
//   2. That content is treated as raw BYTES — no UTF-8 round-trip — so
//      PNG/JPG/BMP/TIFF decode correctly even with bytes >= 0x80, which a
//      text round-trip would mangle (the original IDE imread bug).
//
// We stage real encoded images (produced by imwrite to a temp file) into an
// in-memory VirtualFS that exposes ONLY the binary surface; its text
// readFile() THROWS, so any code path that mistakenly reads the image as
// text fails the test loudly. The image is then read back through an
// explicit `mem:` scheme.

#include <numkit/core/engine.hpp>
#include <numkit/fs/vfs.hpp>
#include <gtest/gtest.h>

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

class MemFS final : public numkit::VirtualFS
{
public:
    std::map<std::string, std::string> files;  // path -> raw bytes

    // Text read must never be used for an image — fail loudly if it is.
    std::string readFile(const std::string &p) override
    {
        throw std::runtime_error("MemFS::readFile must not be used for imread (" + p + ")");
    }
    void writeFile(const std::string &p, const std::string &c) override { files[p] = c; }
    bool exists(const std::string &p) override { return files.count(p) != 0; }
    std::string name() const override { return "mem"; }

    // The binary surface imread/imfinfo actually use.
    std::string readFileBytes(const std::string &p) override
    {
        auto it = files.find(p);
        if (it == files.end())
            throw std::runtime_error("MemFS: no such file " + p);
        return it->second;
    }
    // The binary surface imwrite actually uses.
    void writeFileBytes(const std::string &p, const std::string &bytes) override
    {
        files[p] = bytes;
    }
};

}  // namespace

class ImreadVfsTest : public ::testing::Test
{
public:
    numkit::StdEngine engine;
    MemFS *mem = nullptr;

    void SetUp() override
    {
        engine.eval("import compat.*;");
        auto fs = std::make_unique<MemFS>();
        mem = fs.get();
        engine.registerVirtualFS(std::move(fs));
    }

    // Generate `A` and imwrite it to a real temp file (stb / TIFF writer),
    // slurp the encoded bytes, stage them in the MemFS under `vkey`, delete
    // the temp file. Leaves `A` in the workspace for an isequal compare.
    void stage(const std::string &genMat, const std::string &ext, const std::string &vkey)
    {
        engine.eval(genMat + " p = [tempname '" + ext + "']; imwrite(A, p);");
        const std::string disk = engine.eval("p").toString();
        std::ifstream f(disk, std::ios::binary);
        ASSERT_TRUE(static_cast<bool>(f)) << "could not open imwrite temp file: " << disk;
        std::ostringstream ss;
        ss << f.rdbuf();
        f.close();
        mem->files[vkey] = ss.str();
        engine.eval("delete(p);");
    }

    double sc(const std::string &c) { return engine.eval(c).toScalar(); }
};

// PNG (lossless) RGB round-trips through the binary VFS hook byte-exact.
TEST_F(ImreadVfsTest, PngRgbThroughBinaryHook)
{
    stage("A = uint8(reshape(0:47, 4, 4, 3));", ".png", "rgb.png");
    engine.eval("B = imread('mem:rgb.png');");
    EXPECT_EQ(engine.eval("class(B)").toString(), "uint8");
    EXPECT_DOUBLE_EQ(sc("size(B,1)"), 4.0);
    EXPECT_DOUBLE_EQ(sc("size(B,2)"), 4.0);
    EXPECT_DOUBLE_EQ(sc("size(B,3)"), 3.0);
    EXPECT_DOUBLE_EQ(sc("isequal(A, B)"), 1.0);
}

// PNG grayscale — single-channel decode path.
TEST_F(ImreadVfsTest, PngGrayThroughBinaryHook)
{
    stage("A = uint8(reshape(1:16, 4, 4));", ".png", "g.png");
    engine.eval("B = imread('mem:g.png');");
    EXPECT_DOUBLE_EQ(sc("size(B,3)"), 1.0);
    EXPECT_DOUBLE_EQ(sc("isequal(A, B)"), 1.0);
}

// BMP — a different stb decoder, also lossless. The pixel pattern packs
// many bytes >= 0x80, which a UTF-8 round-trip would corrupt.
TEST_F(ImreadVfsTest, BmpHighBitBytesThroughBinaryHook)
{
    stage("A = uint8(reshape(mod((0:47)*5 + 200, 256), 4, 4, 3));", ".bmp", "x.bmp");
    engine.eval("B = imread('mem:x.bmp');");
    EXPECT_DOUBLE_EQ(sc("size(B,3)"), 3.0);
    EXPECT_DOUBLE_EQ(sc("isequal(A, B)"), 1.0);
}

// TIFF via the VFS — confirms the readTiff(buffer) overload decodes from
// bytes (not a path), routed by isTiffBytes().
TEST_F(ImreadVfsTest, TiffThroughBinaryHook)
{
    stage("A = uint8(reshape(0:47, 4, 4, 3));", ".tif", "x.tif");
    engine.eval("B = imread('mem:x.tif');");
    EXPECT_DOUBLE_EQ(sc("size(B,3)"), 3.0);
    EXPECT_DOUBLE_EQ(sc("isequal(A, B)"), 1.0);
}

// Missing file → clean error, not a crash.
TEST_F(ImreadVfsTest, MissingFileThrows)
{
    EXPECT_THROW(engine.eval("imread('mem:nope.png');"), std::exception);
}

// ── imwrite through the VFS (binary write hook) ──────────────────────
//
// imwrite must encode to bytes and write via writeFileBytes — never fopen.
// We write into the MemFS and read back through the same FS.

// PNG (lossless) RGB written and read back via the VFS round-trips exact.
TEST_F(ImreadVfsTest, ImwritePngRoundTripThroughVfs)
{
    engine.eval("A = uint8(reshape(0:47, 4, 4, 3));");
    engine.eval("imwrite(A, 'mem:out.png');");
    ASSERT_TRUE(mem->files.count("out.png") != 0) << "imwrite did not reach the VFS";
    engine.eval("B = imread('mem:out.png');");
    EXPECT_DOUBLE_EQ(sc("size(B,3)"), 3.0);
    EXPECT_DOUBLE_EQ(sc("isequal(A, B)"), 1.0);
}

// BMP RGB with high-bit bytes — write + read via VFS, byte-exact. (stb's
// BMP writer always emits 24-bit RGB, so we use a 3-channel source to keep
// the channel count stable across the round-trip.)
TEST_F(ImreadVfsTest, ImwriteBmpRoundTripThroughVfs)
{
    engine.eval("A = uint8(reshape(mod((0:47)*5 + 130, 256), 4, 4, 3));");
    engine.eval("imwrite(A, 'mem:out.bmp');");
    engine.eval("B = imread('mem:out.bmp');");
    EXPECT_DOUBLE_EQ(sc("size(B,3)"), 3.0);
    EXPECT_DOUBLE_EQ(sc("isequal(A, B)"), 1.0);
}

// TIFF written + read back via VFS.
TEST_F(ImreadVfsTest, ImwriteTiffRoundTripThroughVfs)
{
    engine.eval("A = uint8(reshape(0:47, 4, 4, 3));");
    engine.eval("imwrite(A, 'mem:out.tif');");
    engine.eval("B = imread('mem:out.tif');");
    EXPECT_DOUBLE_EQ(sc("size(B,3)"), 3.0);
    EXPECT_DOUBLE_EQ(sc("isequal(A, B)"), 1.0);
}

// Multi-page TIFF append: the second imwrite must read the existing pages
// back THROUGH the VFS (readFileBytes), append an IFD, and write again.
TEST_F(ImreadVfsTest, ImwriteTiffAppendMultiPageThroughVfs)
{
    engine.eval("A1 = uint8(reshape(1:16, 4, 4));");
    engine.eval("A2 = uint8(reshape(100:115, 4, 4));");
    engine.eval("imwrite(A1, 'mem:m.tif', 'Compression', 'none');");
    engine.eval("imwrite(A2, 'mem:m.tif', 'Compression', 'none', 'WriteMode', 'append');");
    engine.eval("B1 = imread('mem:m.tif', 1); B2 = imread('mem:m.tif', 2);");
    EXPECT_DOUBLE_EQ(sc("B1(1,1)"),   1.0);
    EXPECT_DOUBLE_EQ(sc("B1(4,4)"),  16.0);
    EXPECT_DOUBLE_EQ(sc("B2(1,1)"), 100.0);
    EXPECT_DOUBLE_EQ(sc("B2(4,4)"), 115.0);
}

// ── imfinfo through the VFS ───────────────────────────────────────────

TEST_F(ImreadVfsTest, ImfinfoPngThroughVfs)
{
    stage("A = uint8(reshape(0:47, 4, 4, 3));", ".png", "i.png");
    engine.eval("s = imfinfo('mem:i.png');");
    EXPECT_DOUBLE_EQ(sc("s.Width"),  4.0);
    EXPECT_DOUBLE_EQ(sc("s.Height"), 4.0);
    EXPECT_DOUBLE_EQ(sc("s.NumberOfChannels"), 3.0);
    EXPECT_EQ(engine.eval("s.Format").toString(), "png");
    EXPECT_DOUBLE_EQ(sc("s.FileSize > 0"), 1.0);
}

TEST_F(ImreadVfsTest, ImfinfoTiffThroughVfs)
{
    stage("A = uint8(reshape(1:16, 4, 4));", ".tif", "i.tif");
    engine.eval("s = imfinfo('mem:i.tif');");
    EXPECT_DOUBLE_EQ(sc("s.Width"),  4.0);
    EXPECT_DOUBLE_EQ(sc("s.Height"), 4.0);
    EXPECT_DOUBLE_EQ(sc("s.NumberOfChannels"), 1.0);
    EXPECT_EQ(engine.eval("s.Format").toString(), "tif");
}

TEST_F(ImreadVfsTest, ImfinfoMissingFileThrows)
{
    EXPECT_THROW(engine.eval("imfinfo('mem:nope.png');"), std::exception);
}
