// toolboxes/image/src/io/tiff_codec.cpp
//
// Production TIFF / BigTIFF reader and writer.
// Zero-dependency pure C++20 implementation.
//
// Coverage:
//   - Header endian detection (II little-endian / MM big-endian)
//   - Classic TIFF (magic 42) and BigTIFF (magic 43, 64-bit offsets)
//   - Compression schemes:
//       * 1: None (uncompressed strips/tiles)
//       * 5: LZW (per TIFF 6.0 appendix F with early change)
//       * 8 / 32946: Deflate (via in-tree deflate/zlib engine)
//       * 32773: PackBits (byte run-length encoding)
//   - Predictor 2: Horizontal differencing for 8/16/32-bit integer samples
//   - Photometric: 0 (WhiteIsZero), 1 (BlackIsZero), 2 (RGB/RGBA), 3 (Palette/Indexed)
//   - SamplesPerPixel: 1, 3, 4; BitsPerSample: 8, 16, 32, 64
//   - SampleFormat: 1=uint, 2=int, 3=float (all supported)
//   - Layout: Chunky and PlanarConfiguration=2; Strips and Tiles
//   - Multi-page: reading arbitrary IFD pages and appending pages

#include "tiff_codec.hpp"
#include "deflate.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace numkit::image {

bool isTiffBytes(const std::uint8_t *data, std::size_t len)
{
    if (len < 4) return false;
    if (data[0] == 'I' && data[1] == 'I') {
        return (data[2] == 42 && data[3] == 0) || (data[2] == 43 && data[3] == 0);
    }
    if (data[0] == 'M' && data[1] == 'M') {
        return (data[2] == 0 && data[3] == 42) || (data[2] == 0 && data[3] == 43);
    }
    return false;
}

bool isTiffBytes(const std::string &b)
{
    return isTiffBytes(reinterpret_cast<const std::uint8_t *>(b.data()), b.size());
}

namespace {

// ============================================================================
// 1. ByteReader & Tag Constants (Endian-aware)
// ============================================================================

struct ByteReader {
    const std::uint8_t *buf;
    std::size_t size;
    bool bigEndian;
    bool isBigTiff;   // BigTIFF (magic 43, 8-byte offsets, 20-byte IFD entries)

    void check(std::size_t off, std::size_t n, const char *what) const {
        if (off > size || off + n > size)
            throw Error(std::string("tiff: out-of-bounds read ") + what,
                        0, 0, "imread", "", "numkit:imread:tiffEOF");
    }
    std::uint16_t u16(std::size_t off) const {
        check(off, 2, "u16");
        const std::uint8_t a = buf[off], b = buf[off + 1];
        return bigEndian ? static_cast<std::uint16_t>((a << 8) | b)
                         : static_cast<std::uint16_t>(a | (b << 8));
    }
    std::uint32_t u32(std::size_t off) const {
        check(off, 4, "u32");
        const std::uint8_t a = buf[off], b = buf[off + 1],
                            c = buf[off + 2], d = buf[off + 3];
        return bigEndian
            ? (std::uint32_t(a) << 24) | (std::uint32_t(b) << 16) |
              (std::uint32_t(c) << 8)  |  std::uint32_t(d)
            :  std::uint32_t(a) |
              (std::uint32_t(b) << 8)  | (std::uint32_t(c) << 16) |
              (std::uint32_t(d) << 24);
    }
    std::uint64_t u64(std::size_t off) const {
        check(off, 8, "u64");
        std::uint64_t v = 0;
        if (bigEndian) {
            for (int i = 0; i < 8; ++i)
                v = (v << 8) | buf[off + i];
        } else {
            for (int i = 0; i < 8; ++i)
                v |= static_cast<std::uint64_t>(buf[off + i]) << (8 * i);
        }
        return v;
    }
    // BigTIFF-aware offset read (4 bytes for classic, 8 bytes for BigTIFF).
    std::uint64_t offsetAt(std::size_t off) const {
        return isBigTiff ? u64(off) : static_cast<std::uint64_t>(u32(off));
    }
};

// TIFF tag type sizes. Indices 1..18 are valid (TIFF 6 + BigTIFF extensions).
constexpr std::size_t kTypeWidth[19] = {
    0,
    1,   // 1  BYTE
    1,   // 2  ASCII
    2,   // 3  SHORT
    4,   // 4  LONG
    8,   // 5  RATIONAL
    1,   // 6  SBYTE
    1,   // 7  UNDEFINED
    2,   // 8  SSHORT
    4,   // 9  SLONG
    8,   // 10 SRATIONAL
    4,   // 11 FLOAT
    8,   // 12 DOUBLE
    4,   // 13 IFD (LONG-sized IFD pointer)
    0,   // 14 unused
    0,   // 15 unused
    8,   // 16 LONG8       (BigTIFF)
    8,   // 17 SLONG8      (BigTIFF)
    8,   // 18 IFD8        (BigTIFF, 8-byte IFD pointer)
};

std::vector<std::uint64_t>
readEntryValues(const ByteReader &br, std::uint16_t type,
                std::uint64_t count, std::uint64_t valueOffset,
                std::size_t entryOffset)
{
    if (type == 0 || type > 18 || kTypeWidth[type] == 0)
        throw Error("tiff: unknown tag type " + std::to_string(type),
                    0, 0, "imread", "", "numkit:imread:tiffType");
    const std::size_t w     = kTypeWidth[type];
    const std::size_t total = w * static_cast<std::size_t>(count);
    const std::size_t inlineCap   = br.isBigTiff ? 8u : 4u;
    const std::size_t inlineSlotOff = br.isBigTiff ? 12u : 8u;
    const std::size_t base = (total <= inlineCap)
        ? (entryOffset + inlineSlotOff)
        : static_cast<std::size_t>(valueOffset);

    std::vector<std::uint64_t> out;
    out.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        const std::size_t off = base + static_cast<std::size_t>(i) * w;
        switch (type) {
            case 1: case 6: case 7: case 2:
                br.check(off, 1, "byte");
                out.push_back(br.buf[off]); break;
            case 3: case 8:  out.push_back(br.u16(off)); break;
            case 4: case 9: case 11: case 13:
                out.push_back(br.u32(off)); break;
            case 16: case 17: case 18:
                out.push_back(br.u64(off)); break;
            default:         out.push_back(0); break;
        }
    }
    return out;
}

struct TiffImage {
    std::uint32_t width = 0, height = 0;
    std::uint16_t bitsPerSample = 8;
    std::uint16_t compression = 1;
    std::uint16_t photometric = 1;
    std::uint16_t samplesPerPixel = 1;
    std::uint32_t rowsPerStrip = 0;
    std::vector<std::uint64_t> stripOffsets;
    std::vector<std::uint64_t> stripByteCounts;
    std::uint16_t planarConfig = 1;
    std::uint16_t sampleFormat = 1;
    std::uint16_t predictor = 1;
    std::vector<std::uint64_t> colorMap;
    std::uint32_t tileWidth = 0, tileLength = 0;
    std::vector<std::uint64_t> tileOffsets;
    std::vector<std::uint64_t> tileByteCounts;
};

TiffImage parseIFD(const ByteReader &br, std::size_t ifdOffset,
                    std::uint64_t *nextIfdOffset)
{
    TiffImage img;
    const std::uint64_t n = br.isBigTiff ? br.u64(ifdOffset) : br.u16(ifdOffset);
    const std::size_t entrySize = br.isBigTiff ? 20u : 12u;
    const std::size_t countOff  = 4;
    const std::size_t voffOff   = br.isBigTiff ? 12u : 8u;
    std::size_t e = ifdOffset + (br.isBigTiff ? 8u : 2u);
    for (std::uint64_t i = 0; i < n; ++i, e += entrySize) {
        const std::uint16_t tag   = br.u16(e + 0);
        const std::uint16_t type  = br.u16(e + 2);
        const std::uint64_t count = br.isBigTiff ? br.u64(e + countOff)
                                                  : static_cast<std::uint64_t>(br.u32(e + countOff));
        const std::uint64_t voff  = br.isBigTiff ? br.u64(e + voffOff)
                                                  : static_cast<std::uint64_t>(br.u32(e + voffOff));
        auto firstVal = [&]() -> std::uint64_t {
            if (count == 0) return 0;
            const auto v = readEntryValues(br, type, count, voff, e);
            return v.empty() ? 0u : v[0];
        };
        switch (tag) {
            case 256: img.width  = static_cast<std::uint32_t>(firstVal()); break;
            case 257: img.height = static_cast<std::uint32_t>(firstVal()); break;
            case 258: img.bitsPerSample = static_cast<std::uint16_t>(firstVal()); break;
            case 259: img.compression = static_cast<std::uint16_t>(firstVal()); break;
            case 262: img.photometric = static_cast<std::uint16_t>(firstVal()); break;
            case 273: img.stripOffsets = readEntryValues(br, type, count, voff, e); break;
            case 277: img.samplesPerPixel = static_cast<std::uint16_t>(firstVal()); break;
            case 278: img.rowsPerStrip = static_cast<std::uint32_t>(firstVal()); break;
            case 279: img.stripByteCounts = readEntryValues(br, type, count, voff, e); break;
            case 284: img.planarConfig = static_cast<std::uint16_t>(firstVal()); break;
            case 317: img.predictor    = static_cast<std::uint16_t>(firstVal()); break;
            case 320: img.colorMap     = readEntryValues(br, type, count, voff, e); break;
            case 322: img.tileWidth    = static_cast<std::uint32_t>(firstVal()); break;
            case 323: img.tileLength   = static_cast<std::uint32_t>(firstVal()); break;
            case 324: img.tileOffsets  = readEntryValues(br, type, count, voff, e); break;
            case 325: img.tileByteCounts = readEntryValues(br, type, count, voff, e); break;
            case 339: img.sampleFormat = static_cast<std::uint16_t>(firstVal()); break;
            default: break;
        }
    }
    if (nextIfdOffset) *nextIfdOffset = br.offsetAt(e);
    return img;
}

// ============================================================================
// 2. Decompression Algorithms (PackBits, LZW, Deflate)
// ============================================================================

std::vector<std::uint8_t>
decodePackBits(const std::uint8_t *src, std::size_t srcLen, std::size_t outHint)
{
    std::vector<std::uint8_t> out;
    out.reserve(outHint > 0 ? outHint : srcLen * 2);
    std::size_t i = 0;
    while (i < srcLen) {
        const std::int8_t n = static_cast<std::int8_t>(src[i++]);
        if (n == -128) continue;                          // no-op
        if (n >= 0) {                                     // literal run
            const std::size_t count = static_cast<std::size_t>(n) + 1;
            if (i + count > srcLen)
                throw Error("tiff: PackBits literal run truncated",
                            0, 0, "imread", "", "numkit:imread:tiffPackBits");
            out.insert(out.end(), src + i, src + i + count);
            i += count;
        } else {                                          // replicate run
            const std::size_t count = static_cast<std::size_t>(-n) + 1;
            if (i >= srcLen)
                throw Error("tiff: PackBits replicate truncated",
                            0, 0, "imread", "", "numkit:imread:tiffPackBits");
            out.insert(out.end(), count, src[i++]);
        }
    }
    return out;
}

class LzwDecoder
{
public:
    LzwDecoder(const std::uint8_t *src, std::size_t len, std::size_t outHint)
        : src_(src), len_(len), bitPos_(0)
    {
        out_.reserve(outHint > 0 ? outHint : len * 4);
    }

    std::vector<std::uint8_t> decode()
    {
        resetDict();
        std::uint32_t prevCode = NoCode;
        while (bitPos_ + codeWidth_ <= len_ * 8) {
            const std::uint32_t code = readBits(codeWidth_);
            if (code == EoiCode) break;
            if (code == ClearCode) {
                resetDict();
                prevCode = NoCode;
                continue;
            }
            std::vector<std::uint8_t> entry;
            if (code < nextCode_) {
                entry = dict_[code];
            } else if (code == nextCode_ && prevCode != NoCode) {
                entry = dict_[prevCode];
                entry.push_back(entry[0]);
            } else {
                throw Error("tiff: LZW invalid code " + std::to_string(code),
                            0, 0, "imread", "", "numkit:imread:tiffLZW");
            }
            out_.insert(out_.end(), entry.begin(), entry.end());

            if (prevCode != NoCode && nextCode_ < kMaxCode) {
                std::vector<std::uint8_t> ne = dict_[prevCode];
                ne.push_back(entry[0]);
                dict_.push_back(std::move(ne));
                ++nextCode_;
                if (nextCode_ + 1 == (1u << codeWidth_) && codeWidth_ < 12)
                    ++codeWidth_;
            }
            prevCode = code;
        }
        return std::move(out_);
    }

private:
    static constexpr std::uint32_t ClearCode = 256;
    static constexpr std::uint32_t EoiCode   = 257;
    static constexpr std::uint32_t NoCode    = 0xFFFFFFFFu;
    static constexpr std::uint32_t kMaxCode  = 4096;

    const std::uint8_t *src_;
    std::size_t         len_;
    std::size_t         bitPos_;
    int                 codeWidth_ = 9;
    std::uint32_t       nextCode_  = 258;
    std::vector<std::vector<std::uint8_t>> dict_;
    std::vector<std::uint8_t>              out_;

    void resetDict() {
        dict_.clear();
        dict_.reserve(kMaxCode);
        for (int c = 0; c < 256; ++c)
            dict_.push_back({static_cast<std::uint8_t>(c)});
        dict_.push_back({});
        dict_.push_back({});
        nextCode_ = 258;
        codeWidth_ = 9;
    }

    std::uint32_t readBits(int n) {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i) {
            const std::size_t bytePos = bitPos_ / 8;
            const int         bitOff  = 7 - static_cast<int>(bitPos_ % 8);
            if (bytePos >= len_) break;
            const std::uint32_t bit = (src_[bytePos] >> bitOff) & 1u;
            v = (v << 1) | bit;
            ++bitPos_;
        }
        return v;
    }
};

std::vector<std::uint8_t>
decodeLZW(const std::uint8_t *src, std::size_t srcLen, std::size_t outHint)
{
    return LzwDecoder(src, srcLen, outHint).decode();
}

std::vector<std::uint8_t>
decodeDeflate(const std::uint8_t *src, std::size_t srcLen, std::size_t outHint)
{
    return zlibDecompress(src, srcLen, outHint);
}

void applyHorizontalUndiff(std::vector<std::uint8_t> &buf,
                            std::size_t H, std::size_t W,
                            std::size_t spp, std::size_t bps)
{
    const std::size_t rowBytes = W * spp * bps;
    if (bps == 1) {
        for (std::size_t r = 0; r < H; ++r) {
            std::uint8_t *row = buf.data() + r * rowBytes;
            for (std::size_t c = 1; c < W; ++c)
                for (std::size_t s = 0; s < spp; ++s)
                    row[c * spp + s] = static_cast<std::uint8_t>(
                        row[c * spp + s] + row[(c - 1) * spp + s]);
        }
    } else if (bps == 2) {
        for (std::size_t r = 0; r < H; ++r) {
            std::uint8_t *row = buf.data() + r * rowBytes;
            for (std::size_t c = 1; c < W; ++c)
                for (std::size_t s = 0; s < spp; ++s) {
                    const std::size_t off = (c * spp + s) * 2;
                    const std::size_t pof = ((c - 1) * spp + s) * 2;
                    std::uint16_t cur, prv;
                    std::memcpy(&cur, row + off, 2);
                    std::memcpy(&prv, row + pof, 2);
                    const std::uint16_t v = static_cast<std::uint16_t>(cur + prv);
                    std::memcpy(row + off, &v, 2);
                }
        }
    } else if (bps == 4) {
        for (std::size_t r = 0; r < H; ++r) {
            std::uint8_t *row = buf.data() + r * rowBytes;
            for (std::size_t c = 1; c < W; ++c)
                for (std::size_t s = 0; s < spp; ++s) {
                    const std::size_t off = (c * spp + s) * 4;
                    const std::size_t pof = ((c - 1) * spp + s) * 4;
                    std::uint32_t cur, prv;
                    std::memcpy(&cur, row + off, 4);
                    std::memcpy(&prv, row + pof, 4);
                    const std::uint32_t v = cur + prv;
                    std::memcpy(row + off, &v, 4);
                }
        }
    }
}

void decompressBlock(const ByteReader &br, std::size_t off, std::size_t cnt,
                     std::uint16_t compression, std::size_t hintBytes,
                     std::vector<std::uint8_t> &out)
{
    br.check(off, cnt, "blockData");
    const std::uint8_t *src = br.buf + off;
    switch (compression) {
        case 1:
            out.insert(out.end(), src, src + cnt);
            break;
        case 32773: {
            auto dec = decodePackBits(src, cnt, hintBytes);
            out.insert(out.end(), dec.begin(), dec.end());
            break;
        }
        case 5: {
            auto dec = decodeLZW(src, cnt, hintBytes);
            out.insert(out.end(), dec.begin(), dec.end());
            break;
        }
        case 8:
        case 32946: {  // Deflate / Adobe-Deflate
            auto dec = decodeDeflate(src, cnt, hintBytes);
            out.insert(out.end(), dec.begin(), dec.end());
            break;
        }
        default:
            throw Error("tiff: compression " + std::to_string(compression)
                        + " not supported",
                        0, 0, "imread", "", "numkit:imread:tiffCompression");
    }
}

std::vector<std::uint8_t>
decodeImage(const ByteReader &br, const TiffImage &img)
{
    const std::size_t H = img.height, W = img.width, S = img.samplesPerPixel;
    const std::size_t bps = img.bitsPerSample / 8;
    const std::size_t rowBytes = W * S * bps;
    const std::size_t totalBytes = rowBytes * H;

    const bool tiled = (img.tileWidth > 0 && img.tileLength > 0);
    const bool planar = (img.planarConfig == 2);

    std::vector<std::uint8_t> dst(totalBytes, 0);

    auto putChunky = [&](const std::uint8_t *plane, std::size_t r0,
                         std::size_t c0, std::size_t rh, std::size_t cw,
                         std::size_t srcStride) {
        for (std::size_t r = 0; r < rh; ++r) {
            const std::size_t outR = r0 + r;
            if (outR >= H) break;
            const std::size_t copyCols = std::min(cw, W - c0);
            const std::uint8_t *sRow = plane + r * srcStride;
            std::uint8_t *dRow = dst.data() + outR * rowBytes
                                  + c0 * S * bps;
            std::memcpy(dRow, sRow, copyCols * S * bps);
        }
    };

    auto putPlanar = [&](const std::uint8_t *plane, std::size_t r0,
                          std::size_t c0, std::size_t rh, std::size_t cw,
                          std::size_t s, std::size_t srcStride) {
        for (std::size_t r = 0; r < rh; ++r) {
            const std::size_t outR = r0 + r;
            if (outR >= H) break;
            const std::size_t copyCols = std::min(cw, W - c0);
            const std::uint8_t *sRow = plane + r * srcStride;
            for (std::size_t c = 0; c < copyCols; ++c) {
                const std::size_t outIdx =
                    outR * rowBytes + (c0 + c) * S * bps + s * bps;
                std::memcpy(dst.data() + outIdx, sRow + c * bps, bps);
            }
        }
    };

    const std::size_t numPlanes = planar ? S : 1;
    const std::size_t spp_block = planar ? 1 : S;

    if (tiled) {
        const std::size_t tilesAcross = (W + img.tileWidth - 1) / img.tileWidth;
        const std::size_t tilesDown   = (H + img.tileLength - 1) / img.tileLength;
        const std::size_t tilesPerPlane = tilesAcross * tilesDown;
        if (img.tileOffsets.size() != tilesPerPlane * numPlanes
            || img.tileByteCounts.size() != tilesPerPlane * numPlanes)
            throw Error("tiff: malformed tile arrays", 0, 0, "imread", "",
                        "numkit:imread:tiffTiles");
        const std::size_t tileBytes = static_cast<std::size_t>(img.tileWidth)
                                       * img.tileLength * spp_block * bps;
        const std::size_t srcStride = img.tileWidth * spp_block * bps;
        for (std::size_t p = 0; p < numPlanes; ++p) {
            for (std::size_t ty = 0; ty < tilesDown; ++ty) {
                for (std::size_t tx = 0; tx < tilesAcross; ++tx) {
                    const std::size_t i = p * tilesPerPlane
                                          + ty * tilesAcross + tx;
                    std::vector<std::uint8_t> tile;
                    tile.reserve(tileBytes);
                    decompressBlock(br, img.tileOffsets[i],
                                    img.tileByteCounts[i], img.compression,
                                    tileBytes, tile);
                    if (tile.size() < tileBytes) tile.resize(tileBytes, 0);
                    const std::size_t r0 = ty * img.tileLength;
                    const std::size_t c0 = tx * img.tileWidth;
                    if (planar)
                        putPlanar(tile.data(), r0, c0,
                                  img.tileLength, img.tileWidth, p, srcStride);
                    else
                        putChunky(tile.data(), r0, c0,
                                  img.tileLength, img.tileWidth, srcStride);
                }
            }
        }
    } else {
        if (img.stripOffsets.empty()
            || img.stripOffsets.size() != img.stripByteCounts.size())
            throw Error("tiff: malformed StripOffsets / StripByteCounts",
                        0, 0, "imread", "", "numkit:imread:tiffStrips");
        const std::size_t stripsPerPlane = img.stripOffsets.size() / numPlanes;
        if (img.stripOffsets.size() % numPlanes != 0)
            throw Error("tiff: strip count not divisible by plane count",
                        0, 0, "imread", "", "numkit:imread:tiffStrips");
        const std::size_t rpsEff = (img.rowsPerStrip > 0
                                     && img.rowsPerStrip < 0x80000000u)
                                     ? img.rowsPerStrip : H;
        const std::size_t srcStride = W * spp_block * bps;
        for (std::size_t p = 0; p < numPlanes; ++p) {
            for (std::size_t si = 0; si < stripsPerPlane; ++si) {
                const std::size_t globalIdx = p * stripsPerPlane + si;
                const std::size_t r0 = si * rpsEff;
                const std::size_t rh = std::min(rpsEff, H - r0);
                const std::size_t hint = srcStride * rh;
                std::vector<std::uint8_t> strip;
                strip.reserve(hint);
                decompressBlock(br, img.stripOffsets[globalIdx],
                                img.stripByteCounts[globalIdx],
                                img.compression, hint, strip);
                if (strip.size() < hint) strip.resize(hint, 0);
                if (planar)
                    putPlanar(strip.data(), r0, 0, rh, W, p, srcStride);
                else
                    putChunky(strip.data(), r0, 0, rh, W, srcStride);
            }
        }
    }

    if (img.predictor == 2)
        applyHorizontalUndiff(dst, H, W, S, bps);
    else if (img.predictor != 1)
        throw Error("tiff: Predictor " + std::to_string(img.predictor)
                    + " not supported (only 1=none and 2=horizontal)",
                    0, 0, "imread", "", "numkit:imread:tiffPredictor");
    return dst;
}

Value rowMajorToValue(const std::vector<std::uint8_t> &raw,
                      const TiffImage &img, bool be,
                      std::pmr::memory_resource *mr)
{
    const std::size_t H = img.height, W = img.width;
    const std::size_t S = img.samplesPerPixel;
    const std::size_t bps = img.bitsPerSample / 8;

    ValueType vt;
    if (img.sampleFormat == 3) {  // IEEE float
        if (img.bitsPerSample == 32)      vt = ValueType::SINGLE;
        else if (img.bitsPerSample == 64) vt = ValueType::DOUBLE;
        else throw Error("tiff: float SampleFormat needs 32 or 64 bits",
                         0, 0, "imread", "", "numkit:imread:tiffBits");
    } else if (img.sampleFormat == 2) {   // signed int
        switch (img.bitsPerSample) {
            case 8:  vt = ValueType::INT8;  break;
            case 16: vt = ValueType::INT16; break;
            case 32: vt = ValueType::INT32; break;
            default: throw Error("tiff: int SampleFormat needs 8/16/32 bits",
                                 0, 0, "imread", "", "numkit:imread:tiffBits");
        }
    } else {                              // unsigned int (or unspecified)
        switch (img.bitsPerSample) {
            case 8:  vt = ValueType::UINT8;  break;
            case 16: vt = ValueType::UINT16; break;
            case 32: vt = ValueType::UINT32; break;
            default: throw Error("tiff: uint SampleFormat needs 8/16/32 bits",
                                 0, 0, "imread", "", "numkit:imread:tiffBits");
        }
    }

    Value out = (S == 1)
        ? Value::matrix(H, W, vt, mr)
        : Value::matrix3d(H, W, S, vt, mr);

    const std::size_t plane = H * W;
    const bool invertGray = (img.photometric == 0);  // WhiteIsZero

    auto fetch = [&](std::size_t r, std::size_t c, std::size_t s) -> std::uint64_t {
        const std::size_t off = ((r * W + c) * S + s) * bps;
        if (bps == 1) return raw[off];
        if (bps == 2) {
            const std::uint16_t lo = raw[off];
            const std::uint16_t hi = raw[off + 1];
            return be ? static_cast<std::uint16_t>((lo << 8) | hi)
                      : static_cast<std::uint16_t>(lo | (hi << 8));
        }
        std::uint64_t v = 0;
        if (be) {
            for (std::size_t k = 0; k < bps; ++k)
                v = (v << 8) | raw[off + k];
        } else {
            for (std::size_t k = 0; k < bps; ++k)
                v |= static_cast<std::uint64_t>(raw[off + k]) << (8 * k);
        }
        return v;
    };

    auto storeAt = [&](std::size_t r, std::size_t c, std::size_t s,
                       std::uint64_t bits) {
        const std::size_t dst = (S == 1) ? (c * H + r)
                                         : (s * plane + c * H + r);
        switch (vt) {
            case ValueType::UINT8: {
                std::uint8_t v = static_cast<std::uint8_t>(bits);
                if (invertGray) v = static_cast<std::uint8_t>(255 - v);
                out.uint8DataMut()[dst] = v; break;
            }
            case ValueType::UINT16: {
                std::uint16_t v = static_cast<std::uint16_t>(bits);
                if (invertGray) v = static_cast<std::uint16_t>(65535 - v);
                out.uint16DataMut()[dst] = v; break;
            }
            case ValueType::UINT32:
                out.uint32DataMut()[dst] = static_cast<std::uint32_t>(bits);
                break;
            case ValueType::INT8: {
                std::int8_t v;
                std::uint8_t u = static_cast<std::uint8_t>(bits);
                std::memcpy(&v, &u, 1);
                out.int8DataMut()[dst] = v; break;
            }
            case ValueType::INT16: {
                std::int16_t v;
                std::uint16_t u = static_cast<std::uint16_t>(bits);
                std::memcpy(&v, &u, 2);
                out.int16DataMut()[dst] = v; break;
            }
            case ValueType::INT32: {
                std::int32_t v;
                std::uint32_t u = static_cast<std::uint32_t>(bits);
                std::memcpy(&v, &u, 4);
                out.int32DataMut()[dst] = v; break;
            }
            case ValueType::SINGLE: {
                float f;
                std::uint32_t u = static_cast<std::uint32_t>(bits);
                std::memcpy(&f, &u, 4);
                out.singleDataMut()[dst] = f; break;
            }
            case ValueType::DOUBLE: {
                double d;
                std::memcpy(&d, &bits, 8);
                out.doubleDataMut()[dst] = d; break;
            }
            default: break;
        }
    };

    for (std::size_t r = 0; r < H; ++r)
        for (std::size_t c = 0; c < W; ++c)
            for (std::size_t s = 0; s < S; ++s)
                storeAt(r, c, s, fetch(r, c, s));
    return out;
}

std::vector<std::uint8_t> loadBytes(const std::string &path, const char *who)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw Error(std::string(who) + ": cannot open '" + path + "'",
                    0, 0, who, "", std::string("numkit:") + who + ":open");
    f.seekg(0, std::ios::end);
    const std::streamoff sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if (sz < 8)
        throw Error(std::string(who) + ": file too small for TIFF header",
                    0, 0, who, "", std::string("numkit:") + who + ":tiffShort");
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
    f.read(reinterpret_cast<char *>(buf.data()), sz);
    return buf;
}

ByteReader openTiff(std::vector<std::uint8_t> &buf, const char *who)
{
    bool be;
    if (buf[0] == 'I' && buf[1] == 'I')      be = false;
    else if (buf[0] == 'M' && buf[1] == 'M') be = true;
    else
        throw Error(std::string(who) + ": not a TIFF (bad byte-order mark)",
                    0, 0, who, "", std::string("numkit:") + who + ":tiffMagic");
    ByteReader br{buf.data(), buf.size(), be, /*isBigTiff=*/false};
    const std::uint16_t magic = br.u16(2);
    if (magic == 42) {
        // classic TIFF
    } else if (magic == 43) {
        if (br.u16(4) != 8 || br.u16(6) != 0)
            throw Error(std::string(who) + ": bad BigTIFF header",
                        0, 0, who, "", std::string("numkit:") + who + ":tiffMagic");
        br.isBigTiff = true;
    } else {
        throw Error(std::string(who) + ": bad TIFF magic", 0, 0, who, "",
                    std::string("numkit:") + who + ":tiffMagic");
    }
    return br;
}

inline std::uint64_t firstIFDOffset(const ByteReader &br) {
    return br.isBigTiff ? br.u64(8) : static_cast<std::uint64_t>(br.u32(4));
}

std::uint64_t locateIFDForPage(const ByteReader &br, std::size_t bufSize,
                                std::uint32_t page, const char *who)
{
    if (page == 0)
        throw Error(std::string(who) + ": page index must be >= 1",
                    0, 0, who, "", std::string("numkit:") + who + ":badPage");
    std::uint64_t off = firstIFDOffset(br);
    const std::size_t entrySize = br.isBigTiff ? 20u : 12u;
    const std::size_t countHdr  = br.isBigTiff ? 8u  : 2u;
    for (std::uint32_t p = 1; p <= page; ++p) {
        if (off == 0 || off + countHdr > bufSize)
            throw Error(std::string(who) + ": requested page "
                        + std::to_string(page)
                        + " is beyond end of TIFF (only "
                        + std::to_string(p - 1) + " pages found)",
                        0, 0, who, "", std::string("numkit:") + who + ":pageRange");
        if (p == page) return off;
        const std::uint64_t n = br.isBigTiff
            ? br.u64(static_cast<std::size_t>(off))
            : static_cast<std::uint64_t>(br.u16(static_cast<std::size_t>(off)));
        off = br.offsetAt(static_cast<std::size_t>(off) + countHdr
                           + entrySize * static_cast<std::size_t>(n));
    }
    return off;
}

// ============================================================================
// 3. Compression Encoders for Writing (PackBits, LZW, Deflate)
// ============================================================================

inline void writeU16LE(std::vector<std::uint8_t> &buf, std::size_t off, std::uint16_t v) {
    if (off + 2 > buf.size()) buf.resize(off + 2);
    buf[off]     = static_cast<std::uint8_t>(v & 0xFF);
    buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

inline void writeU32LE(std::vector<std::uint8_t> &buf, std::size_t off, std::uint32_t v) {
    if (off + 4 > buf.size()) buf.resize(off + 4);
    buf[off]     = static_cast<std::uint8_t>(v & 0xFF);
    buf[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    buf[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
    buf[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
}

std::vector<std::uint8_t> encodePackBits(const std::uint8_t *src, std::size_t n)
{
    std::vector<std::uint8_t> out;
    out.reserve(n + n / 64 + 1);
    std::size_t i = 0;
    while (i < n) {
        std::size_t runEnd = i + 1;
        while (runEnd < n && src[runEnd] == src[i] && runEnd - i < 128)
            ++runEnd;
        const std::size_t runLen = runEnd - i;
        if (runLen >= 3) {
            out.push_back(static_cast<std::uint8_t>(
                static_cast<std::int8_t>(-(static_cast<int>(runLen) - 1))));
            out.push_back(src[i]);
            i = runEnd;
            continue;
        }
        std::size_t litEnd = i + 1;
        while (litEnd < n && litEnd - i < 128) {
            if (litEnd + 2 < n && src[litEnd] == src[litEnd + 1]
                && src[litEnd + 1] == src[litEnd + 2])
                break;
            ++litEnd;
        }
        const std::size_t litLen = litEnd - i;
        out.push_back(static_cast<std::uint8_t>(litLen - 1));
        out.insert(out.end(), src + i, src + litEnd);
        i = litEnd;
    }
    return out;
}

class LzwEncoder
{
public:
    std::vector<std::uint8_t> encode(const std::uint8_t *src, std::size_t n)
    {
        out_.clear();
        out_.reserve(n / 2 + 64);
        bitBuf_ = 0;
        bitCount_ = 0;
        dict_.clear();
        dict_.reserve(kMaxEntries);

        codeWidth_ = 9;
        nextCode_  = 258;

        writeBits(ClearCode);

        if (n == 0) {
            writeBits(EoiCode);
            flushBits();
            return std::move(out_);
        }

        std::uint32_t prefix = src[0];
        for (std::size_t i = 1; i < n; ++i) {
            const std::uint8_t c = src[i];
            const std::uint64_t key = (static_cast<std::uint64_t>(prefix) << 8) | c;
            auto it = dict_.find(key);
            if (it != dict_.end()) {
                prefix = it->second;
            } else {
                writeBits(prefix);
                if (nextCode_ < kMaxEntries) {
                    dict_[key] = static_cast<std::uint16_t>(nextCode_++);
                    if (nextCode_ == (1u << codeWidth_) && codeWidth_ < 12)
                        ++codeWidth_;
                } else {
                    writeBits(ClearCode);
                    dict_.clear();
                    nextCode_ = 258;
                    codeWidth_ = 9;
                }
                prefix = c;
            }
        }
        writeBits(prefix);
        writeBits(EoiCode);
        flushBits();
        return std::move(out_);
    }

private:
    static constexpr std::uint32_t ClearCode   = 256;
    static constexpr std::uint32_t EoiCode     = 257;
    static constexpr std::size_t   kMaxEntries = 4096;

    int                        codeWidth_ = 9;
    std::size_t                nextCode_  = 258;
    std::vector<std::uint8_t>  out_;
    std::uint32_t              bitBuf_    = 0;
    int                        bitCount_  = 0;
    std::unordered_map<std::uint64_t, std::uint16_t> dict_;

    void writeBits(std::uint32_t code) {
        bitBuf_ = (bitBuf_ << codeWidth_) | (code & ((1u << codeWidth_) - 1));
        bitCount_ += codeWidth_;
        while (bitCount_ >= 8) {
            bitCount_ -= 8;
            out_.push_back(static_cast<std::uint8_t>(
                (bitBuf_ >> bitCount_) & 0xFF));
        }
    }
    void flushBits() {
        if (bitCount_ > 0) {
            out_.push_back(static_cast<std::uint8_t>(
                (bitBuf_ << (8 - bitCount_)) & 0xFF));
            bitCount_ = 0;
            bitBuf_ = 0;
        }
    }
};

std::vector<std::uint8_t> encodeLZW(const std::uint8_t *src, std::size_t n)
{
    return LzwEncoder().encode(src, n);
}

std::vector<std::uint8_t> encodeDeflate(const std::uint8_t *src, std::size_t n)
{
    return zlibCompress(src, n, 6);
}

struct DtypeMap {
    std::size_t   bytesPerSample;
    std::uint16_t bitsPerSample;
    std::uint16_t sampleFormat;  // 1=uint, 2=int, 3=float
    enum class Mode { U8, U16, U32, I8, I16, I32, F32, F64 } mode;
};

DtypeMap mapDtype(ValueType vt)
{
    switch (vt) {
        case ValueType::UINT8:
        case ValueType::LOGICAL:
        case ValueType::CHAR:
            return { 1, 8,  1, DtypeMap::Mode::U8  };
        case ValueType::UINT16:
            return { 2, 16, 1, DtypeMap::Mode::U16 };
        case ValueType::UINT32:
            return { 4, 32, 1, DtypeMap::Mode::U32 };
        case ValueType::INT8:
            return { 1, 8,  2, DtypeMap::Mode::I8  };
        case ValueType::INT16:
            return { 2, 16, 2, DtypeMap::Mode::I16 };
        case ValueType::INT32:
            return { 4, 32, 2, DtypeMap::Mode::I32 };
        case ValueType::SINGLE:
            return { 4, 32, 3, DtypeMap::Mode::F32 };
        case ValueType::DOUBLE:
            return { 8, 64, 3, DtypeMap::Mode::F64 };
        default:
            return { 1, 8,  1, DtypeMap::Mode::U8  };
    }
}

std::vector<std::uint8_t>
extractChunkyBytes(const Value &A, std::size_t H, std::size_t W,
                   std::size_t S, const DtypeMap &dm)
{
    const std::size_t bps = dm.bytesPerSample;
    const std::size_t total = H * W * S * bps;
    std::vector<std::uint8_t> bytes(total);
    const std::size_t plane = H * W;

    auto writeU16 = [](std::uint8_t *p, std::uint16_t v) {
        p[0] = static_cast<std::uint8_t>(v & 0xFF);
        p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    };
    auto writeU32 = [](std::uint8_t *p, std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    };
    auto writeU64 = [](std::uint8_t *p, std::uint64_t v) {
        for (int i = 0; i < 8; ++i)
            p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    };

    for (std::size_t r = 0; r < H; ++r)
        for (std::size_t c = 0; c < W; ++c)
            for (std::size_t s = 0; s < S; ++s) {
                const std::size_t srcIdx = (S == 1)
                    ? (c * H + r)
                    : (s * plane + c * H + r);
                std::uint8_t *dst = bytes.data() + (r * W + c) * S * bps + s * bps;
                switch (dm.mode) {
                    case DtypeMap::Mode::U8: {
                        double dv = A.elemAsDouble(srcIdx);
                        if (dv < 0) dv = 0; if (dv > 255) dv = 255;
                        *dst = static_cast<std::uint8_t>(static_cast<int>(dv));
                        break;
                    }
                    case DtypeMap::Mode::U16:
                        writeU16(dst, A.uint16Data()[srcIdx]); break;
                    case DtypeMap::Mode::U32:
                        writeU32(dst, A.uint32Data()[srcIdx]); break;
                    case DtypeMap::Mode::I8: {
                        std::int8_t v = A.int8Data()[srcIdx];
                        std::memcpy(dst, &v, 1); break;
                    }
                    case DtypeMap::Mode::I16: {
                        std::int16_t v = A.int16Data()[srcIdx];
                        std::uint16_t u; std::memcpy(&u, &v, 2);
                        writeU16(dst, u); break;
                    }
                    case DtypeMap::Mode::I32: {
                        std::int32_t v = A.int32Data()[srcIdx];
                        std::uint32_t u; std::memcpy(&u, &v, 4);
                        writeU32(dst, u); break;
                    }
                    case DtypeMap::Mode::F32: {
                        float f = A.singleData()[srcIdx];
                        std::uint32_t u; std::memcpy(&u, &f, 4);
                        writeU32(dst, u); break;
                    }
                    case DtypeMap::Mode::F64: {
                        double d = A.doubleData()[srcIdx];
                        std::uint64_t u; std::memcpy(&u, &d, 8);
                        writeU64(dst, u); break;
                    }
                }
            }
    return bytes;
}

void applyHorizontalDiff(std::vector<std::uint8_t> &buf, std::size_t H,
                          std::size_t W, std::size_t S, std::size_t bps)
{
    const std::size_t rowBytes = W * S * bps;
    if (bps == 1) {
        for (std::size_t r = 0; r < H; ++r) {
            std::uint8_t *row = buf.data() + r * rowBytes;
            for (std::size_t c = W; c-- > 1;) {
                for (std::size_t s = 0; s < S; ++s)
                    row[c * S + s] = static_cast<std::uint8_t>(
                        row[c * S + s] - row[(c - 1) * S + s]);
            }
        }
    } else if (bps == 2) {
        for (std::size_t r = 0; r < H; ++r) {
            std::uint8_t *row = buf.data() + r * rowBytes;
            for (std::size_t c = W; c-- > 1;) {
                for (std::size_t s = 0; s < S; ++s) {
                    const std::size_t off = (c * S + s) * 2;
                    const std::size_t pof = ((c - 1) * S + s) * 2;
                    std::uint16_t cur, prv;
                    std::memcpy(&cur, row + off, 2);
                    std::memcpy(&prv, row + pof, 2);
                    const std::uint16_t v = static_cast<std::uint16_t>(cur - prv);
                    std::memcpy(row + off, &v, 2);
                }
            }
        }
    } else if (bps == 4) {
        for (std::size_t r = 0; r < H; ++r) {
            std::uint8_t *row = buf.data() + r * rowBytes;
            for (std::size_t c = W; c-- > 1;) {
                for (std::size_t s = 0; s < S; ++s) {
                    const std::size_t off = (c * S + s) * 4;
                    const std::size_t pof = ((c - 1) * S + s) * 4;
                    std::uint32_t cur, prv;
                    std::memcpy(&cur, row + off, 4);
                    std::memcpy(&prv, row + pof, 4);
                    const std::uint32_t v = cur - prv;
                    std::memcpy(row + off, &v, 4);
                }
            }
        }
    }
}

std::uint16_t parseCompression(const std::string &s)
{
    if (s.empty() || s == "none")     return 1;
    if (s == "packbits")              return 32773;
    if (s == "lzw")                   return 5;
    if (s == "deflate")               return 8;
    throw Error("imwrite TIFF: unknown Compression '" + s + "' (use none, packbits, lzw, or deflate)",
                0, 0, "imwrite", "", "numkit:imwrite:tiffCompression");
}

std::vector<std::uint8_t>
compressStrip(const std::uint8_t *src, std::size_t n, std::uint16_t compression)
{
    switch (compression) {
        case 1:     return std::vector<std::uint8_t>(src, src + n);
        case 32773: return encodePackBits(src, n);
        case 5:     return encodeLZW(src, n);
        case 8:     return encodeDeflate(src, n);
        default:
            throw Error("imwrite TIFF: unsupported compression",
                        0, 0, "imwrite", "", "numkit:imwrite:tiffCompression");
    }
}

void writePage(std::vector<std::uint8_t> &buf, const Value &A,
               std::uint16_t compression,
               std::size_t prevNextIFDOff)
{
    const auto &d = A.dims();
    const std::size_t H = d.rows();
    const std::size_t W = d.cols();
    const std::size_t S = (d.ndim() == 3) ? d.pages() : 1;
    if (H == 0 || W == 0)
        throw Error("imwrite TIFF: empty image", 0, 0, "imwrite", "",
                    "numkit:imwrite:tiffShape");
    if (S != 1 && S != 3 && S != 4)
        throw Error("imwrite TIFF: only 1, 3, or 4 channels supported",
                    0, 0, "imwrite", "", "numkit:imwrite:tiffShape");

    const DtypeMap dm = mapDtype(A.type());
    const std::size_t bps = dm.bytesPerSample;
    auto bytes = extractChunkyBytes(A, H, W, S, dm);

    const bool useHPred = (compression == 5 || compression == 8 || compression == 32946)
                          && (dm.sampleFormat != 3)
                          && (bps == 1 || bps == 2 || bps == 4);
    if (useHPred)
        applyHorizontalDiff(bytes, H, W, S, bps);
    const std::uint16_t predictor = useHPred ? 2u : 1u;

    auto strip = compressStrip(bytes.data(), bytes.size(), compression);

    if ((buf.size() & 1) != 0) buf.push_back(0);
    const std::uint32_t stripOffset = static_cast<std::uint32_t>(buf.size());
    buf.insert(buf.end(), strip.begin(), strip.end());

    if ((buf.size() & 1) != 0) buf.push_back(0);

    const std::uint16_t photometric = (S == 1) ? 1u : 2u;
    const std::uint16_t bitsPerSample = dm.bitsPerSample;

    std::uint32_t bpsArrayOffset = 0;
    if (S > 1) {
        bpsArrayOffset = static_cast<std::uint32_t>(buf.size());
        for (std::size_t s = 0; s < S; ++s) {
            buf.push_back(static_cast<std::uint8_t>(bitsPerSample & 0xFF));
            buf.push_back(static_cast<std::uint8_t>((bitsPerSample >> 8) & 0xFF));
        }
        if ((buf.size() & 1) != 0) buf.push_back(0);
    }

    const std::uint32_t ifdOffset = static_cast<std::uint32_t>(buf.size());

    struct Entry {
        std::uint16_t tag, type;
        std::uint32_t count;
        std::uint32_t value;
    };
    std::vector<Entry> entries;

    entries.push_back({256, 4, 1, static_cast<std::uint32_t>(W)});
    entries.push_back({257, 4, 1, static_cast<std::uint32_t>(H)});
    if (S > 1) {
        entries.push_back({258, 3, static_cast<std::uint32_t>(S), bpsArrayOffset});
    } else {
        entries.push_back({258, 3, 1, bitsPerSample});
    }
    entries.push_back({259, 3, 1, compression});
    entries.push_back({262, 3, 1, photometric});
    entries.push_back({273, 4, 1, stripOffset});
    entries.push_back({277, 3, 1, static_cast<std::uint32_t>(S)});
    entries.push_back({278, 4, 1, static_cast<std::uint32_t>(H)});
    entries.push_back({279, 4, 1, static_cast<std::uint32_t>(strip.size())});
    entries.push_back({284, 3, 1, 1u});                // PlanarConfig chunky
    if (predictor != 1)
        entries.push_back({317, 3, 1, predictor});     // Predictor=2 (horizontal)
    entries.push_back({339, 3, 1, dm.sampleFormat});   // SampleFormat (1/2/3)

    const std::size_t ifdBytes = 2 + entries.size() * 12 + 4;
    const std::size_t ifdEnd = ifdOffset + ifdBytes;
    buf.resize(ifdEnd, 0);

    writeU16LE(buf, ifdOffset, static_cast<std::uint16_t>(entries.size()));
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const std::size_t e = ifdOffset + 2 + i * 12;
        writeU16LE(buf, e + 0, entries[i].tag);
        writeU16LE(buf, e + 2, entries[i].type);
        writeU32LE(buf, e + 4, entries[i].count);
        writeU32LE(buf, e + 8, entries[i].value);
    }
    writeU32LE(buf, ifdEnd - 4, 0u);

    if (prevNextIFDOff != SIZE_MAX)
        writeU32LE(buf, prevNextIFDOff, ifdOffset);
}

} // anonymous

// ============================================================================
// 4. Public API Implementations
// ============================================================================

Value readTiff(const std::string &path, std::pmr::memory_resource *mr)
{
    return readTiff(path, 1u, mr);
}

Value readTiff(const std::string &path, std::uint32_t page,
               std::pmr::memory_resource *mr)
{
    return readTiff(loadBytes(path, "imread"), page, mr);
}

Value readTiff(std::vector<std::uint8_t> buf, std::uint32_t page,
               std::pmr::memory_resource *mr)
{
    auto br = openTiff(buf, "imread");
    const std::uint64_t ifdOff = locateIFDForPage(br, buf.size(), page, "imread");

    std::uint64_t next = 0;
    TiffImage img = parseIFD(br, static_cast<std::size_t>(ifdOff), &next);
    if (img.width == 0 || img.height == 0)
        throw Error("imread: TIFF has zero width or height",
                    0, 0, "imread", "", "numkit:imread:tiffShape");
    if (img.photometric != 0 && img.photometric != 1 && img.photometric != 2
        && img.photometric != 3)
        throw Error("tiff: PhotometricInterpretation "
                    + std::to_string(img.photometric)
                    + " not supported (only 0/1=gray, 2=RGB, 3=palette)",
                    0, 0, "imread", "", "numkit:imread:tiffPhotometric");

    auto raw = decodeImage(br, img);
    return rowMajorToValue(raw, img, br.bigEndian, mr);
}

std::pair<Value, Value>
readTiffWithMap(const std::string &path, std::uint32_t page,
                std::pmr::memory_resource *mr)
{
    return readTiffWithMap(loadBytes(path, "imread"), page, mr);
}

std::pair<Value, Value>
readTiffWithMap(std::vector<std::uint8_t> buf, std::uint32_t page,
                std::pmr::memory_resource *mr)
{
    auto br  = openTiff(buf, "imread");
    const std::uint64_t ifdOff = locateIFDForPage(br, buf.size(), page, "imread");

    std::uint64_t next = 0;
    TiffImage img = parseIFD(br, static_cast<std::size_t>(ifdOff), &next);
    if (img.width == 0 || img.height == 0)
        throw Error("imread: TIFF has zero width or height",
                    0, 0, "imread", "", "numkit:imread:tiffShape");
    if (img.photometric != 0 && img.photometric != 1 && img.photometric != 2
        && img.photometric != 3)
        throw Error("tiff: PhotometricInterpretation "
                    + std::to_string(img.photometric)
                    + " not supported (only 0/1=gray, 2=RGB, 3=palette)",
                    0, 0, "imread", "", "numkit:imread:tiffPhotometric");

    auto raw = decodeImage(br, img);
    Value indices = rowMajorToValue(raw, img, br.bigEndian, mr);

    Value cmap = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (img.photometric == 3) {
        const std::size_t K = std::size_t{1} << img.bitsPerSample;
        if (img.colorMap.size() != 3 * K)
            throw Error("imread: palette TIFF has malformed ColorMap "
                        "(expected 3 × " + std::to_string(K) + " entries)",
                        0, 0, "imread", "", "numkit:imread:tiffColorMap");
        cmap = Value::matrix(K, 3, ValueType::DOUBLE, mr);
        double *cd = cmap.doubleDataMut();
        constexpr double kInv65535 = 1.0 / 65535.0;
        for (std::size_t k = 0; k < K; ++k) {
            cd[0 * K + k] = img.colorMap[0 * K + k] * kInv65535;   // R
            cd[1 * K + k] = img.colorMap[1 * K + k] * kInv65535;   // G
            cd[2 * K + k] = img.colorMap[2 * K + k] * kInv65535;   // B
        }
    }
    return { std::move(indices), std::move(cmap) };
}

void peekTiff(const std::vector<std::uint8_t> &bufIn, std::uint32_t &W,
              std::uint32_t &H, std::uint16_t &bits, std::uint16_t &channels)
{
    std::vector<std::uint8_t> buf = bufIn;
    if (buf.size() < 8)
        throw Error("imfinfo: file too small for TIFF header",
                    0, 0, "imfinfo", "", "numkit:imfinfo:tiffShort");
    auto br = openTiff(buf, "imfinfo");
    std::uint64_t next = 0;
    TiffImage img = parseIFD(br, static_cast<std::size_t>(firstIFDOffset(br)), &next);
    W = img.width; H = img.height;
    bits = img.bitsPerSample;
    channels = img.samplesPerPixel;
}

void peekTiff(const std::string &path, std::uint32_t &W, std::uint32_t &H,
              std::uint16_t &bits, std::uint16_t &channels)
{
    peekTiff(loadBytes(path, "imfinfo"), W, H, bits, channels);
}

std::uint32_t tiffNumPages(const std::string &path)
{
    auto buf = loadBytes(path, "imfinfo");
    auto br = openTiff(buf, "imfinfo");
    const std::size_t entrySize = br.isBigTiff ? 20u : 12u;
    const std::size_t countHdr  = br.isBigTiff ? 8u  : 2u;
    std::uint64_t off = firstIFDOffset(br);
    std::uint32_t n = 0;
    while (off != 0 && static_cast<std::size_t>(off) + countHdr <= buf.size()) {
        ++n;
        const std::uint64_t k = br.isBigTiff
            ? br.u64(static_cast<std::size_t>(off))
            : static_cast<std::uint64_t>(br.u16(static_cast<std::size_t>(off)));
        off = br.offsetAt(static_cast<std::size_t>(off) + countHdr
                           + entrySize * static_cast<std::size_t>(k));
    }
    return n;
}

std::vector<std::uint8_t>
writeTiffToBytes(const Value &A, const std::string &compression,
                 const std::vector<std::uint8_t> *existing)
{
    const std::uint16_t comp = parseCompression(compression);
    std::vector<std::uint8_t> buf;

    if (existing && !existing->empty()) {
        buf = *existing;
        if (buf.size() < 8 || !(buf[0] == 'I' && buf[1] == 'I')
            || buf[2] != 0x2A || buf[3] != 0x00)
            throw Error("imwrite TIFF: append target is not a little-endian TIFF",
                        0, 0, "imwrite", "", "numkit:imwrite:tiffMagic");
        std::uint32_t off = static_cast<std::uint32_t>(
            buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24));
        std::size_t lastNextSlot = 4;
        while (off != 0 && off + 2 <= buf.size()) {
            const std::uint16_t n = static_cast<std::uint16_t>(
                buf[off] | (buf[off + 1] << 8));
            lastNextSlot = off + 2 + 12u * n;
            if (lastNextSlot + 4 > buf.size()) break;
            off = static_cast<std::uint32_t>(
                buf[lastNextSlot]
                | (buf[lastNextSlot + 1] << 8)
                | (buf[lastNextSlot + 2] << 16)
                | (buf[lastNextSlot + 3] << 24));
        }
        writePage(buf, A, comp, lastNextSlot);
    } else {
        buf.assign(8, 0);
        buf[0] = 'I'; buf[1] = 'I';
        writeU16LE(buf, 2, 42);
        writeU32LE(buf, 4, 0);
        writePage(buf, A, comp, /*prevNextIFDOff=*/4);
    }
    return buf;
}

void writeTiff(const Value &A, const std::string &path,
               const std::string &compression,
               bool appendMode)
{
    std::vector<std::uint8_t> existing;
    const std::vector<std::uint8_t> *exptr = nullptr;
    if (appendMode && std::filesystem::exists(path)) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw Error("imwrite TIFF: cannot reopen file for append",
                              0, 0, "imwrite", "", "numkit:imwrite:open");
        in.seekg(0, std::ios::end);
        const std::streamoff sz = in.tellg();
        in.seekg(0, std::ios::beg);
        existing.resize(static_cast<std::size_t>(sz));
        in.read(reinterpret_cast<char *>(existing.data()), sz);
        exptr = &existing;
    }

    const std::vector<std::uint8_t> buf = writeTiffToBytes(A, compression, exptr);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw Error("imwrite TIFF: cannot open '" + path + "' for write",
                          0, 0, "imwrite", "", "numkit:imwrite:open");
    out.write(reinterpret_cast<const char *>(buf.data()), buf.size());
}

} // namespace numkit::image
