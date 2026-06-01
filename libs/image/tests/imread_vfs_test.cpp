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
#include <numkit/core/vfs.hpp>
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

    // The binary surface imread actually uses.
    std::string readFileBytes(const std::string &p) override
    {
        auto it = files.find(p);
        if (it == files.end())
            throw std::runtime_error("MemFS: no such file " + p);
        return it->second;
    }
};

}  // namespace

class ImreadVfsTest : public ::testing::Test
{
public:
    numkit::Engine engine;
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
