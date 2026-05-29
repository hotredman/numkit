// libs/image/src/io/tiff_reader.cpp
//
// Production TIFF reader — Variant 2 of the imread TIFF plan.
//
// Coverage:
//   - Header endian detection (II / MM)
//   - IFD walk (first IFD by default; multi-page via readTiff(path, page))
//   - Compression: 1 (none), 5 (LZW per TIFF 6.0 appendix F), 32773 (PackBits)
//     Deflate (8) deferred to next cycle (needs zlib FetchContent)
//   - Photometric: 0 (WhiteIsZero) [inverted], 1 (BlackIsZero gray),
//     2 (RGB) — palette/CMYK deferred to next cycle
//   - SamplesPerPixel ∈ {1, 3, 4}; BitsPerSample ∈ {8, 16, 32}
//   - SampleFormat 1=uint, 2=int, 3=float (all supported for 8/16/32)
//   - PlanarConfiguration 1 (chunky) only — separate-planes deferred
//   - Strip layout (Tile deferred)
//
// References:
//   - Adobe Systems, "TIFF Revision 6.0" (1992) — base spec
//     https://download.osgeo.org/libtiff/doc/TIFF6.pdf
//   - Adobe TIFF technical notes (compression appendix F)
//   - Welch, "A Technique for High-Performance Data Compression",
//     IEEE Computer 17(6), 1984 — LZW algorithm

#include <numkit/image/io/io.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#ifdef NUMKIT_WITH_ZLIB
#  include <zlib.h>
#endif

namespace numkit::image {

namespace {

// ── byte-order-aware readers ─────────────────────────────────────────

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

// Read all values of an IFD entry as uint64s. Classic and BigTIFF
// differ in:
//   * IFD entry size (12 / 20 bytes)
//   * "inline value" threshold (4 / 8 bytes) and the slot's position
//     within the entry (bytes 8..11 vs 12..19)
//   * The value-field is u32 in classic, u64 in BigTIFF.
//
// Tags whose values we care about (offsets, widths, photometric, etc.)
// fit cleanly into uint64. Float / rational types are not consumed by
// this reader, so we just return zero for those.
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

// ── decoded IFD ──────────────────────────────────────────────────────

struct TiffImage {
    std::uint32_t width = 0, height = 0;
    std::uint16_t bitsPerSample = 8;
    std::uint16_t compression = 1;
    std::uint16_t photometric = 1;
    std::uint16_t samplesPerPixel = 1;
    std::uint32_t rowsPerStrip = 0;
    // Offsets / counts widened to u64 so the same struct serves both
    // classic and BigTIFF without secondary casts.
    std::vector<std::uint64_t> stripOffsets;
    std::vector<std::uint64_t> stripByteCounts;
    std::uint16_t planarConfig = 1;
    std::uint16_t sampleFormat = 1;
    std::uint16_t predictor = 1;
    // Colour map for Photometric=3 (palette): 3·(2^BitsPerSample) uint16
    // values, all R entries then all G then all B.
    std::vector<std::uint64_t> colorMap;
    // Tile layout (tags 322/323/324/325).
    std::uint32_t tileWidth = 0, tileLength = 0;
    std::vector<std::uint64_t> tileOffsets;
    std::vector<std::uint64_t> tileByteCounts;
};

// Parse a single IFD at the given byte offset. Returns the next-IFD
// offset (0 if last) via out-param. Handles both classic and BigTIFF
// entry layouts:
//   classic   : count(u16) + N×12-byte-entries [tag(2),type(2),count(4),value(4)] + next(u32)
//   BigTIFF   : count(u64) + N×20-byte-entries [tag(2),type(2),count(8),value(8)] + next(u64)
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

// ── PackBits decoder (TIFF 6.0 § 9, compression code 32773) ─────────
//
// PackBits is a run-length encoding where each "control byte" n decides
// the next run:
//   n ∈ [0,    127]: copy the next n+1 bytes verbatim
//   n ∈ [-127, -1] : replicate the next byte (-n)+1 times
//   n == -128      : no-op
// Signed interpretation; n is read as int8_t.
std::vector<std::uint8_t>
decodePackBits(const std::uint8_t *src, std::size_t srcLen,
               std::size_t outHint)
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

// ── LZW decoder (TIFF 6.0 appendix F, compression code 5) ────────────
//
// Welch's algorithm with TIFF-specific quirks:
//   - Codes start at 9 bits wide
//   - Code width increases to 10/11/12 bits one step EARLIER than
//     standard LZW (TIFF "early change" off-by-one — this is mandatory
//     per the TIFF 6.0 spec even though it's a known bug)
//   - Reserved codes: 256 = CLEAR (reset dict), 257 = EOI
//   - Dict entries 0..255 are single-byte; 258+ are growing strings
//   - Maximum code = 4093 (12-bit width caps at next-code 4094 due to
//     early change)
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
                // KwKwK case — entry is prevEntry + prevEntry[0].
                entry = dict_[prevCode];
                entry.push_back(entry[0]);
            } else {
                throw Error("tiff: LZW invalid code " + std::to_string(code),
                            0, 0, "imread", "", "numkit:imread:tiffLZW");
            }
            out_.insert(out_.end(), entry.begin(), entry.end());

            if (prevCode != NoCode && nextCode_ < kMaxCode) {
                // Append (prevEntry + entry[0]) as new dict entry.
                std::vector<std::uint8_t> ne = dict_[prevCode];
                ne.push_back(entry[0]);
                dict_.push_back(std::move(ne));
                ++nextCode_;
                // Early-change rule: bump width when next code would
                // exceed (1 << codeWidth_) - 1 *or* when next code equals
                // (1 << codeWidth_) - 1 (TIFF off-by-one).
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
        // Placeholders for the reserved codes 256/257 keep indexing simple.
        dict_.push_back({});
        dict_.push_back({});
        nextCode_ = 258;
        codeWidth_ = 9;
    }

    // MSB-first bit reader.
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

// ── Deflate decoder (zlib-wrapped — TIFF compression 8 / 32946) ─────
//
// TIFF "Deflate" wraps the raw deflate stream in a 2-byte zlib header
// + 4-byte Adler-32 checksum (RFC 1950). zlib's inflateInit() handles
// this transparently. We grow the output buffer as needed.
#ifdef NUMKIT_WITH_ZLIB
std::vector<std::uint8_t>
decodeDeflate(const std::uint8_t *src, std::size_t srcLen, std::size_t outHint)
{
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK)
        throw Error("tiff: zlib inflateInit failed", 0, 0, "imread", "",
                    "numkit:imread:tiffDeflate");
    zs.next_in = const_cast<Bytef *>(src);
    zs.avail_in = static_cast<uInt>(srcLen);

    std::vector<std::uint8_t> out;
    out.resize(outHint > 0 ? outHint : srcLen * 4);
    std::size_t produced = 0;
    while (true) {
        if (produced == out.size())
            out.resize(out.size() * 2);
        zs.next_out  = out.data() + produced;
        zs.avail_out = static_cast<uInt>(out.size() - produced);
        const int rc = inflate(&zs, Z_NO_FLUSH);
        produced = out.size() - zs.avail_out;
        if (rc == Z_STREAM_END) break;
        if (rc != Z_OK) {
            inflateEnd(&zs);
            throw Error(std::string("tiff: Deflate decode failed: ")
                        + (zs.msg ? zs.msg : zError(rc)),
                        0, 0, "imread", "", "numkit:imread:tiffDeflate");
        }
    }
    inflateEnd(&zs);
    out.resize(produced);
    return out;
}
#endif

// ── Horizontal differencing predictor (TIFF tag 317, value 2) ────────
//
// Used with LZW/Deflate to improve compression. The encoder stored
// pred[i] = pixel[i] - pixel[i-1] per scanline (first pixel unchanged).
// We undo by cumulative-summing across each row, per sample component.
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

// ── unified block decoder (strip or tile) ───────────────────────────
//
// Decompress one block (strip or tile) at file offset `off` with byte
// count `cnt`. The caller passes the expected uncompressed `hintBytes`
// (used by decoders to reserve buffers). Result is appended into out.
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
#ifdef NUMKIT_WITH_ZLIB
            auto dec = decodeDeflate(src, cnt, hintBytes);
            out.insert(out.end(), dec.begin(), dec.end());
            break;
#else
            throw Error("tiff: Deflate requires zlib at build time "
                        "(NUMKIT_WITH_ZLIB not defined)",
                        0, 0, "imread", "", "numkit:imread:tiffDeflate");
#endif
        }
        default:
            throw Error("tiff: compression " + std::to_string(compression)
                        + " not supported",
                        0, 0, "imread", "", "numkit:imread:tiffCompression");
    }
}

// Decode the full image (all strips or tiles, chunky or planar) into a
// single row-major chunky-interleaved buffer of size
// H * W * SamplesPerPixel * bytesPerSample.
std::vector<std::uint8_t>
decodeImage(const ByteReader &br, const TiffImage &img)
{
    const std::size_t H = img.height, W = img.width, S = img.samplesPerPixel;
    const std::size_t bps = img.bitsPerSample / 8;
    const std::size_t rowBytes = W * S * bps;
    const std::size_t totalBytes = rowBytes * H;

    const bool tiled = (img.tileWidth > 0 && img.tileLength > 0);
    const bool planar = (img.planarConfig == 2);

    // For predictor application we need a planar-aware decode first if
    // PlanarConfiguration=2; we reassemble at the end.
    std::vector<std::uint8_t> dst(totalBytes, 0);

    auto putChunky = [&](const std::uint8_t *plane, std::size_t r0,
                         std::size_t c0, std::size_t rh, std::size_t cw,
                         std::size_t srcStride) {
        // Copy a rectangular block from a per-tile/per-strip planar
        // buffer (chunky-interleaved, S samples × bps bytes per pixel)
        // into the destination at (r0, c0). `srcStride` is the per-row
        // byte stride within the source block; output stride is rowBytes.
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
        // Source is single-component (bps bytes per pixel); insert into
        // the chunky destination at sample index s.
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
        // Strip-based layout.
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

// ── row-major chunky bytes → numkit column-major Value ───────────────

Value rowMajorToValue(const std::vector<std::uint8_t> &raw,
                      const TiffImage &img, bool be,
                      std::pmr::memory_resource *mr)
{
    const std::size_t H = img.height, W = img.width;
    const std::size_t S = img.samplesPerPixel;
    const std::size_t bps = img.bitsPerSample / 8;

    // Pick output dtype from BitsPerSample + SampleFormat.
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

    // Generic per-element copy with byte-swap if needed for >8-bit.
    auto fetch = [&](std::size_t r, std::size_t c, std::size_t s) -> std::uint64_t {
        const std::size_t off = ((r * W + c) * S + s) * bps;
        if (bps == 1) return raw[off];
        if (bps == 2) {
            const std::uint16_t lo = raw[off];
            const std::uint16_t hi = raw[off + 1];
            return be ? static_cast<std::uint16_t>((lo << 8) | hi)
                      : static_cast<std::uint16_t>(lo | (hi << 8));
        }
        // bps == 4 or 8
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

// ── shared helper: load TIFF file bytes ─────────────────────────────

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
        // classic TIFF — nothing more to set up.
    } else if (magic == 43) {
        // BigTIFF header layout (after byte-order + magic):
        //   bytes 4-5 : bytesPerOffset (always 8)
        //   bytes 6-7 : constant 0
        //   bytes 8-15: first IFD offset (8 bytes)
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

} // anonymous

// Walk the IFD chain to locate the IFD for the given 1-based page index.
// Returns the file offset of that IFD; throws if `page` is out of range.
// BigTIFF-aware first-IFD offset (4 bytes at offset 4 for classic,
// 8 bytes at offset 8 for BigTIFF).
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
        // Skip this IFD to next-IFD offset.
        const std::uint64_t n = br.isBigTiff
            ? br.u64(static_cast<std::size_t>(off))
            : static_cast<std::uint64_t>(br.u16(static_cast<std::size_t>(off)));
        off = br.offsetAt(static_cast<std::size_t>(off) + countHdr
                           + entrySize * static_cast<std::size_t>(n));
    }
    return off;
}

// ── public API ──────────────────────────────────────────────────────

Value readTiff(const std::string &path, std::pmr::memory_resource *mr)
{
    return readTiff(path, 1u, mr);
}

Value readTiff(const std::string &path, std::uint32_t page,
               std::pmr::memory_resource *mr)
{
    auto buf = loadBytes(path, "imread");
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

    // Palette: MATLAB's single-output `imread(file)` for palette TIFFs
    // returns the *indexed* values; the colormap is accessible via
    // imfinfo or the two-output form. We return uint8/uint16 indices
    // unchanged — no palette expansion in the single-output path.
    return rowMajorToValue(raw, img, br.bigEndian, mr);
}

// Two-output API for palette TIFFs. Returns (indices, cmap) where
// `cmap` is K×3 DOUBLE in [0, 1] for Photometric=3, or empty otherwise.
//
// TIFF ColorMap tag (320) layout per spec: `3 · (2^BitsPerSample)` SHORT
// values laid out [all R; all G; all B] in [0, 65535]. We normalise to
// [0, 1] and stack as MATLAB's K×3 cmap.
std::pair<Value, Value>
readTiffWithMap(const std::string &path, std::uint32_t page,
                std::pmr::memory_resource *mr)
{
    auto buf = loadBytes(path, "imread");
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

void peekTiff(const std::string &path, std::uint32_t &W, std::uint32_t &H,
              std::uint16_t &bits, std::uint16_t &channels)
{
    auto buf = loadBytes(path, "imfinfo");
    auto br = openTiff(buf, "imfinfo");
    std::uint64_t next = 0;
    TiffImage img = parseIFD(br, static_cast<std::size_t>(firstIFDOffset(br)), &next);
    W = img.width; H = img.height;
    bits = img.bitsPerSample;
    channels = img.samplesPerPixel;
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

} // namespace numkit::image
