// libs/image/src/io/tiff_reader.cpp
//
// Minimal TIFF reader — Variant 2 of the imread TIFF plan.
//
// Cycle 90 baseline coverage:
//   - Header endian detection (II / MM)
//   - First IFD parsing — entries decoded into a TiffImage struct
//   - Compression = 1 (none) only; throws clearly for other schemes
//   - Photometric = 1 (BlackIsZero / grayscale) or 2 (RGB)
//   - PlanarConfiguration = 1 (chunky / interleaved) only
//   - BitsPerSample = 8 (uint8 output) or 16 (uint16 output)
//   - SamplesPerPixel = 1 (gray) or 3 (RGB)
//   - Single-page (first IFD; multi-page deferred)
//
// Strips:
//   Pixel rows are stored in 1+ strips of `RowsPerStrip` rows each;
//   StripOffsets[i] gives the byte offset of strip i and
//   StripByteCounts[i] its byte length. Rows are row-major within a
//   strip. For chunky layout the samples (RGB) interleave per pixel.
//
// Output layout — TIFF row-major (r, c[, s]) →
//   numkit column-major (r, c[, s]) at linear index
//     s · H · W + c · H + r.
//
// References:
//   - Adobe Systems, "TIFF Revision 6.0", 1992
//     https://download.osgeo.org/libtiff/doc/TIFF6.pdf
//   - libtiff source as cross-check for tag semantics

#include <numkit/image/io/io.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace numkit::image {

namespace {

// ── byte-order-aware readers ─────────────────────────────────────────

struct ByteReader {
    const std::uint8_t *buf;
    std::size_t size;
    bool bigEndian;

    void check(std::size_t off, std::size_t n, const char *what) const {
        if (off > size || off + n > size)
            throw Error(std::string("tiff: out-of-bounds read ") + what,
                        0, 0, "imread", "", "m:imread:tiffEOF");
    }

    std::uint16_t u16(std::size_t off) const {
        check(off, 2, "u16");
        const std::uint8_t a = buf[off];
        const std::uint8_t b = buf[off + 1];
        return bigEndian ? static_cast<std::uint16_t>((a << 8) | b)
                         : static_cast<std::uint16_t>(a | (b << 8));
    }

    std::uint32_t u32(std::size_t off) const {
        check(off, 4, "u32");
        const std::uint8_t a = buf[off];
        const std::uint8_t b = buf[off + 1];
        const std::uint8_t c = buf[off + 2];
        const std::uint8_t d = buf[off + 3];
        return bigEndian
            ? (static_cast<std::uint32_t>(a) << 24) |
              (static_cast<std::uint32_t>(b) << 16) |
              (static_cast<std::uint32_t>(c) << 8)  |
              (static_cast<std::uint32_t>(d))
            : (static_cast<std::uint32_t>(a)) |
              (static_cast<std::uint32_t>(b) << 8)  |
              (static_cast<std::uint32_t>(c) << 16) |
              (static_cast<std::uint32_t>(d) << 24);
    }
};

// TIFF tag-type widths in bytes (1-based).
// Indices 0..12 — only 1..12 are valid per TIFF 6.0.
constexpr std::size_t kTypeWidth[13] = {
    0,  // unused
    1,  // BYTE
    1,  // ASCII
    2,  // SHORT
    4,  // LONG
    8,  // RATIONAL (2 × LONG)
    1,  // SBYTE
    1,  // UNDEFINED
    2,  // SSHORT
    4,  // SLONG
    8,  // SRATIONAL
    4,  // FLOAT
    8,  // DOUBLE
};

// Read N count values of the given type, returning each as a uint32_t
// (sufficient for the integer-valued tags we use here). For RATIONAL
// the numerator/denominator pair is collapsed to numerator/denominator
// rounded to uint32 — none of the tags we care about use RATIONAL.
std::vector<std::uint32_t>
readEntryValues(const ByteReader &br, std::uint16_t type,
                std::uint32_t count, std::uint32_t valueOffset,
                std::size_t entryOffset)
{
    if (type == 0 || type > 12)
        throw Error("tiff: unknown tag type " + std::to_string(type),
                    0, 0, "imread", "", "m:imread:tiffType");
    const std::size_t w = kTypeWidth[type];
    const std::size_t total = w * static_cast<std::size_t>(count);
    // If total ≤ 4 bytes, the value is packed into the entry's
    // valueOffset slot (bytes 8..11 of the 12-byte entry).
    const std::size_t base = (total <= 4)
        ? (entryOffset + 8)
        : static_cast<std::size_t>(valueOffset);

    std::vector<std::uint32_t> out;
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t off = base + i * w;
        switch (type) {
            case 1:  // BYTE
            case 6:  // SBYTE
            case 7:  // UNDEFINED
                br.check(off, 1, "byte");
                out.push_back(br.buf[off]);
                break;
            case 3:  // SHORT
            case 8:  // SSHORT
                out.push_back(br.u16(off));
                break;
            case 4:  // LONG
            case 9:  // SLONG
                out.push_back(br.u32(off));
                break;
            case 2:  // ASCII — pack the byte verbatim (used rarely here)
                br.check(off, 1, "ascii");
                out.push_back(br.buf[off]);
                break;
            default:
                // RATIONAL / FLOAT / DOUBLE — not needed for the tags we
                // consume; skip to a single placeholder.
                out.push_back(0);
                break;
        }
    }
    return out;
}

// ── decoded IFD ──────────────────────────────────────────────────────

struct TiffImage {
    std::uint32_t width          = 0;
    std::uint32_t height         = 0;
    std::uint16_t bitsPerSample  = 8;
    std::uint16_t compression    = 1;
    std::uint16_t photometric    = 1;     // 1=BlackIsZero (gray), 2=RGB
    std::uint16_t samplesPerPixel = 1;
    std::uint32_t rowsPerStrip   = 0;
    std::vector<std::uint32_t> stripOffsets;
    std::vector<std::uint32_t> stripByteCounts;
    std::uint16_t planarConfig   = 1;
    std::uint16_t sampleFormat   = 1;     // 1=uint, 2=int, 3=float
};

TiffImage parseFirstIFD(const ByteReader &br, std::size_t ifdOffset)
{
    TiffImage img;
    const std::uint16_t n = br.u16(ifdOffset);
    std::size_t e = ifdOffset + 2;
    for (std::uint16_t i = 0; i < n; ++i, e += 12) {
        const std::uint16_t tag   = br.u16(e + 0);
        const std::uint16_t type  = br.u16(e + 2);
        const std::uint32_t count = br.u32(e + 4);
        const std::uint32_t voff  = br.u32(e + 8);
        // Cheap path for single-value SHORT/LONG: read directly.
        auto firstVal = [&]() -> std::uint32_t {
            if (count == 0) return 0;
            const auto v = readEntryValues(br, type, count, voff, e);
            return v.empty() ? 0u : v[0];
        };
        switch (tag) {
            case 256: img.width  = firstVal(); break;       // ImageWidth
            case 257: img.height = firstVal(); break;       // ImageLength
            case 258: {                                       // BitsPerSample
                // May be an array (one per sample); take the first —
                // baseline mandates same bits per sample.
                img.bitsPerSample = static_cast<std::uint16_t>(firstVal());
                break;
            }
            case 259: img.compression = static_cast<std::uint16_t>(firstVal()); break;
            case 262: img.photometric = static_cast<std::uint16_t>(firstVal()); break;
            case 273: img.stripOffsets = readEntryValues(br, type, count, voff, e); break;
            case 277: img.samplesPerPixel = static_cast<std::uint16_t>(firstVal()); break;
            case 278: img.rowsPerStrip = firstVal(); break;
            case 279: img.stripByteCounts = readEntryValues(br, type, count, voff, e); break;
            case 284: img.planarConfig = static_cast<std::uint16_t>(firstVal()); break;
            case 339: img.sampleFormat = static_cast<std::uint16_t>(firstVal()); break;
            default: break;  // ignore unknown tags
        }
    }
    return img;
}

// Decode chunky strip into a row-major byte buffer (size = H * W * SPP * bytesPerSample).
std::vector<std::uint8_t>
decodeStrips(const ByteReader &br, const TiffImage &img)
{
    if (img.compression != 1)
        throw Error("tiff: compression " + std::to_string(img.compression)
                    + " not supported in this revision (only uncompressed)",
                    0, 0, "imread", "", "m:imread:tiffCompression");
    if (img.planarConfig != 1)
        throw Error("tiff: PlanarConfiguration=2 (planar) not supported",
                    0, 0, "imread", "", "m:imread:tiffPlanar");
    if (img.stripOffsets.empty() || img.stripByteCounts.empty()
        || img.stripOffsets.size() != img.stripByteCounts.size())
        throw Error("tiff: malformed StripOffsets / StripByteCounts",
                    0, 0, "imread", "", "m:imread:tiffStrips");

    const std::size_t bytesPerSample = img.bitsPerSample / 8;
    const std::size_t expected = static_cast<std::size_t>(img.width)
                                  * static_cast<std::size_t>(img.height)
                                  * static_cast<std::size_t>(img.samplesPerPixel)
                                  * bytesPerSample;
    std::vector<std::uint8_t> raw;
    raw.reserve(expected);
    for (std::size_t i = 0; i < img.stripOffsets.size(); ++i) {
        const std::size_t off = img.stripOffsets[i];
        const std::size_t cnt = img.stripByteCounts[i];
        br.check(off, cnt, "stripData");
        raw.insert(raw.end(), br.buf + off, br.buf + off + cnt);
    }
    if (raw.size() < expected)
        throw Error("tiff: strip data truncated", 0, 0, "imread", "",
                    "m:imread:tiffTruncated");
    raw.resize(expected);
    return raw;
}

// Convert row-major chunky (TIFF) bytes into numkit column-major Value.
Value rowMajorToValue(const std::vector<std::uint8_t> &raw,
                      const TiffImage &img,
                      std::pmr::memory_resource *mr)
{
    const std::size_t H = img.height;
    const std::size_t W = img.width;
    const std::size_t S = img.samplesPerPixel;
    const std::size_t bps = img.bitsPerSample / 8;

    ValueType vt;
    if (img.bitsPerSample == 8) {
        vt = ValueType::UINT8;
    } else if (img.bitsPerSample == 16) {
        vt = ValueType::UINT16;
    } else {
        throw Error("tiff: BitsPerSample " + std::to_string(img.bitsPerSample)
                    + " not supported in this revision (8 or 16 only)",
                    0, 0, "imread", "", "m:imread:tiffBits");
    }

    Value out;
    if (S == 1) {
        out = Value::matrix(H, W, vt, mr);
    } else if (S == 3 || S == 4) {
        out = Value::matrix3d(H, W, S, vt, mr);
    } else {
        throw Error("tiff: SamplesPerPixel " + std::to_string(S)
                    + " not supported (must be 1, 3, or 4)",
                    0, 0, "imread", "", "m:imread:tiffSamples");
    }

    // For each output pixel (r, c, s) we look up TIFF (r, c, s) row-major.
    // Source linear index = (r * W + c) * S * bps + s * bps.
    // Destination column-major index = s * H * W + c * H + r.
    if (img.bitsPerSample == 8) {
        std::uint8_t *od = out.uint8DataMut();
        const std::size_t plane = H * W;
        for (std::size_t r = 0; r < H; ++r)
            for (std::size_t c = 0; c < W; ++c)
                for (std::size_t s = 0; s < S; ++s) {
                    const std::size_t src = (r * W + c) * S + s;
                    const std::size_t dst = (S == 1)
                        ? (c * H + r)
                        : (s * plane + c * H + r);
                    od[dst] = raw[src];
                }
    } else {  // 16-bit
        std::uint16_t *od = out.uint16DataMut();
        const std::size_t plane = H * W;
        for (std::size_t r = 0; r < H; ++r)
            for (std::size_t c = 0; c < W; ++c)
                for (std::size_t s = 0; s < S; ++s) {
                    const std::size_t srcByte = ((r * W + c) * S + s) * 2;
                    // Little/big endian inside each sample word.
                    const std::uint16_t v = static_cast<std::uint16_t>(
                        img.bitsPerSample == 16
                          ? (raw[srcByte] |
                             (static_cast<std::uint16_t>(raw[srcByte + 1]) << 8))
                          : raw[srcByte]);
                    const std::size_t dst = (S == 1)
                        ? (c * H + r)
                        : (s * plane + c * H + r);
                    od[dst] = v;
                }
    }
    return out;
}

} // anonymous

// ── public entry called from imread() when TIFF magic is detected ───
//
// Returns Value of UINT8 / UINT16 in numkit column-major layout.
Value readTiff(const std::string &path, std::pmr::memory_resource *mr)
{
    // Load the entire file into memory. TIFFs we target are small —
    // this keeps the offset arithmetic trivial. (A future iteration
    // can switch to mmap or chunked I/O if huge files come up.)
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw Error("imread: cannot open '" + path + "'",
                    0, 0, "imread", "", "m:imread:open");
    f.seekg(0, std::ios::end);
    const std::streamoff sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if (sz < 8)
        throw Error("imread: file too small for TIFF header",
                    0, 0, "imread", "", "m:imread:tiffShort");
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
    f.read(reinterpret_cast<char *>(buf.data()), sz);
    if (!f)
        throw Error("imread: read failed for '" + path + "'",
                    0, 0, "imread", "", "m:imread:read");

    // Byte order: 'II' little-endian, 'MM' big-endian.
    bool be;
    if (buf[0] == 'I' && buf[1] == 'I')      be = false;
    else if (buf[0] == 'M' && buf[1] == 'M') be = true;
    else
        throw Error("imread: '" + path + "' is not a TIFF (bad byte-order mark)",
                    0, 0, "imread", "", "m:imread:tiffMagic");

    ByteReader br{buf.data(), buf.size(), be};

    // Magic 42 + first IFD offset.
    if (br.u16(2) != 42)
        throw Error("imread: bad TIFF magic", 0, 0, "imread", "",
                    "m:imread:tiffMagic");
    const std::uint32_t ifd0 = br.u32(4);
    if (ifd0 == 0 || ifd0 + 2 > buf.size())
        throw Error("imread: bad first-IFD offset", 0, 0, "imread", "",
                    "m:imread:tiffIFD");

    TiffImage img = parseFirstIFD(br, ifd0);
    if (img.width == 0 || img.height == 0)
        throw Error("imread: TIFF has zero width or height",
                    0, 0, "imread", "", "m:imread:tiffShape");
    if (img.photometric != 1 && img.photometric != 2)
        throw Error("tiff: PhotometricInterpretation "
                    + std::to_string(img.photometric)
                    + " not supported (only 1=BlackIsZero or 2=RGB)",
                    0, 0, "imread", "", "m:imread:tiffPhotometric");

    auto raw = decodeStrips(br, img);
    return rowMajorToValue(raw, img, mr);
}

// Cheap metadata-only path for imfinfo. Reuses the IFD parser but
// skips the strip decode.
void peekTiff(const std::string &path, std::uint32_t &W, std::uint32_t &H,
              std::uint16_t &bits, std::uint16_t &channels)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw Error("imfinfo: cannot open '" + path + "'",
                    0, 0, "imfinfo", "", "m:imfinfo:open");
    f.seekg(0, std::ios::end);
    const std::streamoff sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if (sz < 8)
        throw Error("imfinfo: file too small for TIFF header",
                    0, 0, "imfinfo", "", "m:imfinfo:tiffShort");
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
    f.read(reinterpret_cast<char *>(buf.data()), sz);

    bool be;
    if (buf[0] == 'I' && buf[1] == 'I')      be = false;
    else if (buf[0] == 'M' && buf[1] == 'M') be = true;
    else
        throw Error("imfinfo: not a TIFF", 0, 0, "imfinfo", "",
                    "m:imfinfo:tiffMagic");

    ByteReader br{buf.data(), buf.size(), be};
    if (br.u16(2) != 42)
        throw Error("imfinfo: bad TIFF magic", 0, 0, "imfinfo", "",
                    "m:imfinfo:tiffMagic");
    TiffImage img = parseFirstIFD(br, br.u32(4));
    W        = img.width;
    H        = img.height;
    bits     = img.bitsPerSample;
    channels = img.samplesPerPixel;
}

} // namespace numkit::image
