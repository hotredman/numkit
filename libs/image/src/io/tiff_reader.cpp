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
};

constexpr std::size_t kTypeWidth[13] = {
    0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8
};

std::vector<std::uint32_t>
readEntryValues(const ByteReader &br, std::uint16_t type,
                std::uint32_t count, std::uint32_t valueOffset,
                std::size_t entryOffset)
{
    if (type == 0 || type > 12)
        throw Error("tiff: unknown tag type " + std::to_string(type),
                    0, 0, "imread", "", "numkit:imread:tiffType");
    const std::size_t w = kTypeWidth[type];
    const std::size_t total = w * static_cast<std::size_t>(count);
    const std::size_t base = (total <= 4)
        ? (entryOffset + 8)
        : static_cast<std::size_t>(valueOffset);

    std::vector<std::uint32_t> out;
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t off = base + i * w;
        switch (type) {
            case 1: case 6: case 7: case 2:
                br.check(off, 1, "byte");
                out.push_back(br.buf[off]); break;
            case 3: case 8:  out.push_back(br.u16(off)); break;
            case 4: case 9:  out.push_back(br.u32(off)); break;
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
    std::vector<std::uint32_t> stripOffsets;
    std::vector<std::uint32_t> stripByteCounts;
    std::uint16_t planarConfig = 1;
    std::uint16_t sampleFormat = 1;  // 1=uint, 2=int, 3=float
    std::uint16_t predictor = 1;     // 1=none, 2=horizontal differencing
};

// Parse a single IFD at the given byte offset. Returns the next-IFD
// offset (0 if last) via out-param.
TiffImage parseIFD(const ByteReader &br, std::size_t ifdOffset,
                    std::uint32_t *nextIfdOffset)
{
    TiffImage img;
    const std::uint16_t n = br.u16(ifdOffset);
    std::size_t e = ifdOffset + 2;
    for (std::uint16_t i = 0; i < n; ++i, e += 12) {
        const std::uint16_t tag   = br.u16(e + 0);
        const std::uint16_t type  = br.u16(e + 2);
        const std::uint32_t count = br.u32(e + 4);
        const std::uint32_t voff  = br.u32(e + 8);
        auto firstVal = [&]() -> std::uint32_t {
            if (count == 0) return 0;
            const auto v = readEntryValues(br, type, count, voff, e);
            return v.empty() ? 0u : v[0];
        };
        switch (tag) {
            case 256: img.width  = firstVal(); break;
            case 257: img.height = firstVal(); break;
            case 258: img.bitsPerSample = static_cast<std::uint16_t>(firstVal()); break;
            case 259: img.compression = static_cast<std::uint16_t>(firstVal()); break;
            case 262: img.photometric = static_cast<std::uint16_t>(firstVal()); break;
            case 273: img.stripOffsets = readEntryValues(br, type, count, voff, e); break;
            case 277: img.samplesPerPixel = static_cast<std::uint16_t>(firstVal()); break;
            case 278: img.rowsPerStrip = firstVal(); break;
            case 279: img.stripByteCounts = readEntryValues(br, type, count, voff, e); break;
            case 284: img.planarConfig = static_cast<std::uint16_t>(firstVal()); break;
            case 317: img.predictor    = static_cast<std::uint16_t>(firstVal()); break;
            case 339: img.sampleFormat = static_cast<std::uint16_t>(firstVal()); break;
            default: break;
        }
    }
    if (nextIfdOffset) *nextIfdOffset = br.u32(e);
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

// ── strip decoder ────────────────────────────────────────────────────

std::vector<std::uint8_t>
decodeStrips(const ByteReader &br, const TiffImage &img)
{
    if (img.planarConfig != 1)
        throw Error("tiff: PlanarConfiguration=2 (planar) not yet supported",
                    0, 0, "imread", "", "numkit:imread:tiffPlanar");
    if (img.stripOffsets.empty() || img.stripByteCounts.empty()
        || img.stripOffsets.size() != img.stripByteCounts.size())
        throw Error("tiff: malformed StripOffsets / StripByteCounts",
                    0, 0, "imread", "", "numkit:imread:tiffStrips");

    const std::size_t bytesPerSample = img.bitsPerSample / 8;
    const std::size_t rowBytes = static_cast<std::size_t>(img.width)
                                  * img.samplesPerPixel * bytesPerSample;
    const std::size_t expected = rowBytes * img.height;

    std::vector<std::uint8_t> raw;
    raw.reserve(expected);
    for (std::size_t i = 0; i < img.stripOffsets.size(); ++i) {
        const std::size_t off = img.stripOffsets[i];
        const std::size_t cnt = img.stripByteCounts[i];
        br.check(off, cnt, "stripData");
        const std::uint8_t *src = br.buf + off;
        const std::size_t  stripRows = std::min<std::size_t>(
            img.rowsPerStrip,
            img.height - i * std::max<std::size_t>(1, img.rowsPerStrip));
        const std::size_t  stripHint = rowBytes * (stripRows > 0 ? stripRows
                                                                 : img.rowsPerStrip);

        switch (img.compression) {
            case 1: {  // none
                raw.insert(raw.end(), src, src + cnt);
                break;
            }
            case 32773: {  // PackBits
                auto dec = decodePackBits(src, cnt, stripHint);
                raw.insert(raw.end(), dec.begin(), dec.end());
                break;
            }
            case 5: {  // LZW
                auto dec = decodeLZW(src, cnt, stripHint);
                raw.insert(raw.end(), dec.begin(), dec.end());
                break;
            }
            case 8:
            case 32946:  // Adobe-Deflate alias
                throw Error("tiff: Deflate compression not yet supported "
                            "(planned for next cycle — needs zlib FetchContent)",
                            0, 0, "imread", "", "numkit:imread:tiffDeflate");
            default:
                throw Error("tiff: compression " + std::to_string(img.compression)
                            + " not supported",
                            0, 0, "imread", "",
                            "numkit:imread:tiffCompression");
        }
    }
    if (raw.size() < expected)
        throw Error("tiff: strip data truncated", 0, 0, "imread", "",
                    "numkit:imread:tiffTruncated");
    raw.resize(expected);

    if (img.predictor == 2)
        applyHorizontalUndiff(raw, img.height, img.width,
                               img.samplesPerPixel, bytesPerSample);
    else if (img.predictor != 1)
        throw Error("tiff: Predictor " + std::to_string(img.predictor)
                    + " not supported (only 1=none and 2=horizontal)",
                    0, 0, "imread", "", "numkit:imread:tiffPredictor");
    return raw;
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
    ByteReader br{buf.data(), buf.size(), be};
    if (br.u16(2) != 42)
        throw Error(std::string(who) + ": bad TIFF magic", 0, 0, who, "",
                    std::string("numkit:") + who + ":tiffMagic");
    return br;
}

} // anonymous

// ── public API ──────────────────────────────────────────────────────

Value readTiff(const std::string &path, std::pmr::memory_resource *mr)
{
    auto buf = loadBytes(path, "imread");
    auto br = openTiff(buf, "imread");
    std::uint32_t ifdOff = br.u32(4);
    if (ifdOff == 0 || ifdOff + 2 > buf.size())
        throw Error("imread: bad first-IFD offset", 0, 0, "imread", "",
                    "numkit:imread:tiffIFD");

    std::uint32_t next = 0;
    TiffImage img = parseIFD(br, ifdOff, &next);
    if (img.width == 0 || img.height == 0)
        throw Error("imread: TIFF has zero width or height",
                    0, 0, "imread", "", "numkit:imread:tiffShape");
    if (img.photometric != 0 && img.photometric != 1 && img.photometric != 2)
        throw Error("tiff: PhotometricInterpretation "
                    + std::to_string(img.photometric)
                    + " not supported (only 0/1=gray, 2=RGB)",
                    0, 0, "imread", "", "numkit:imread:tiffPhotometric");

    auto raw = decodeStrips(br, img);
    return rowMajorToValue(raw, img, br.bigEndian, mr);
}

void peekTiff(const std::string &path, std::uint32_t &W, std::uint32_t &H,
              std::uint16_t &bits, std::uint16_t &channels)
{
    auto buf = loadBytes(path, "imfinfo");
    auto br = openTiff(buf, "imfinfo");
    std::uint32_t next = 0;
    TiffImage img = parseIFD(br, br.u32(4), &next);
    W = img.width; H = img.height;
    bits = img.bitsPerSample;
    channels = img.samplesPerPixel;
}

} // namespace numkit::image
