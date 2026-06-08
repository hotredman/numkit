// toolboxes/image/src/io/tiff_writer.cpp
//
// Production TIFF writer (Variant 2 cycle C). Companion to tiff_reader.cpp.
//
// Coverage:
//   - Compression: none (1), PackBits (32773), LZW (5), Deflate (8)
//   - Photometric: 1 (BlackIsZero gray), 2 (RGB)
//   - SamplesPerPixel: 1 (gray), 3 (RGB), 4 (RGBA)
//   - BitsPerSample: 8, 16
//   - PlanarConfiguration = 1 (chunky)
//   - Single strip per page (rowsPerStrip = image height)
//   - Multi-page: append mode appends a new IFD pointing the previous
//     IFD's "next IFD" slot at it
//   - Little-endian byte order ('II'), the form MATLAB also writes
//
// Output layout per single-page file:
//   [TIFF header: II + 42 + first IFD offset]
//   [pixel strip(s) — written before IFD so the IFD can reference them]
//   [IFD]
//
// References:
//   - Adobe Systems, "TIFF Revision 6.0" (1992)

#include <numkit/image/io/io.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef NUMKIT_WITH_ZLIB
#  include <zlib.h>
#endif

namespace numkit::image {

namespace {

// ── helpers ────────────────────────────────────────────────────────

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

// ── compression encoders ───────────────────────────────────────────

// PackBits encoder. Greedy run/literal grouping (TIFF 6.0 spec compliant).
std::vector<std::uint8_t> encodePackBits(const std::uint8_t *src, std::size_t n)
{
    std::vector<std::uint8_t> out;
    out.reserve(n + n / 64 + 1);
    std::size_t i = 0;
    while (i < n) {
        // Detect a run of identical bytes (length ≥ 3 → encode as replicate).
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
        // Otherwise emit a literal run up to the next run-of-3-or-more or
        // the end of input, capped at 128 bytes.
        std::size_t litEnd = i + 1;
        while (litEnd < n && litEnd - i < 128) {
            // Stop if a 3-byte run starts here.
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

// LZW encoder (TIFF flavour with early-change bit-width bump).
// Welch '84 with TIFF appendix-F dictionary semantics.
class LzwEncoder
{
public:
    LzwEncoder() { reset(); }
    std::vector<std::uint8_t> encode(const std::uint8_t *src, std::size_t n) {
        writeCode(ClearCode);
        std::vector<std::uint8_t> w;
        for (std::size_t i = 0; i < n; ++i) {
            std::vector<std::uint8_t> wk = w;
            wk.push_back(src[i]);
            auto it = dict_.find(wk);
            if (it != dict_.end()) {
                w = std::move(wk);
                continue;
            }
            // Output code for w, add wk to dict, set w = src[i].
            writeCode(dict_.at(w));
            if (nextCode_ < kMaxCode) {
                dict_[wk] = nextCode_++;
                // Early-change rule: bump width BEFORE nextCode fills the
                // current width.
                if (nextCode_ + 1 == (1u << codeWidth_) && codeWidth_ < 12)
                    ++codeWidth_;
                else if (nextCode_ == (1u << codeWidth_) && codeWidth_ < 12) {
                    // Defensive: keep bump tied to "next code fills width".
                    ++codeWidth_;
                }
                if (nextCode_ == kMaxCode) {
                    // Dictionary full — emit CLEAR.
                    writeCode(ClearCode);
                    reset();
                }
            }
            w = { src[i] };
        }
        if (!w.empty())
            writeCode(dict_.at(w));
        writeCode(EoiCode);
        flushBits();
        return std::move(out_);
    }

private:
    static constexpr std::uint32_t ClearCode = 256;
    static constexpr std::uint32_t EoiCode   = 257;
    static constexpr std::uint32_t kMaxCode  = 4094;

    struct VecHash {
        std::size_t operator()(const std::vector<std::uint8_t> &v) const {
            // 64-bit FNV-1a, computed in uint64 then narrowed to size_t so the
            // offset-basis constant is not truncated where size_t is 32-bit.
            std::uint64_t h = 1469598103934665603ull;
            for (auto b : v) {
                h ^= b;
                h *= 1099511628211ull;
            }
            return static_cast<std::size_t>(h);
        }
    };
    std::unordered_map<std::vector<std::uint8_t>, std::uint32_t, VecHash> dict_;
    int           codeWidth_ = 9;
    std::uint32_t nextCode_  = 258;
    std::uint64_t bitBuf_    = 0;
    int           bitCount_  = 0;
    std::vector<std::uint8_t> out_;

    void reset() {
        dict_.clear();
        dict_.reserve(kMaxCode);
        for (int c = 0; c < 256; ++c)
            dict_[std::vector<std::uint8_t>{static_cast<std::uint8_t>(c)}] =
                static_cast<std::uint32_t>(c);
        nextCode_ = 258;
        codeWidth_ = 9;
    }

    void writeCode(std::uint32_t code) {
        bitBuf_ = (bitBuf_ << codeWidth_) | code;
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

#ifdef NUMKIT_WITH_ZLIB
std::vector<std::uint8_t> encodeDeflate(const std::uint8_t *src, std::size_t n)
{
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK)
        throw Error("imwrite: zlib deflateInit failed",
                    0, 0, "imwrite", "", "numkit:imwrite:tiffDeflate");
    zs.next_in = const_cast<Bytef *>(src);
    zs.avail_in = static_cast<uInt>(n);

    const std::size_t bound = deflateBound(&zs, static_cast<uLong>(n));
    std::vector<std::uint8_t> out(bound);
    zs.next_out  = out.data();
    zs.avail_out = static_cast<uInt>(out.size());

    const int rc = deflate(&zs, Z_FINISH);
    const std::size_t produced = out.size() - zs.avail_out;
    deflateEnd(&zs);
    if (rc != Z_STREAM_END)
        throw Error(std::string("imwrite: Deflate encode failed: ")
                    + (zs.msg ? zs.msg : zError(rc)),
                    0, 0, "imwrite", "", "numkit:imwrite:tiffDeflate");
    out.resize(produced);
    return out;
}
#endif

// ── chunky row-major bytes from numkit Value ───────────────────────

// Map an input ValueType to TIFF (BitsPerSample, SampleFormat). Native
// MATLAB-image types (uint8/16, int8/16/32, single, double, logical)
// map 1:1. Generic numeric inputs that don't have a natural integer
// repr default to uint8 (with clamp), preserving the pre-cycle-92
// behaviour.
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
        case ValueType::UINT16:  return { 2, 16, 1, DtypeMap::Mode::U16 };
        case ValueType::UINT32:  return { 4, 32, 1, DtypeMap::Mode::U32 };
        case ValueType::INT8:    return { 1, 8,  2, DtypeMap::Mode::I8  };
        case ValueType::INT16:   return { 2, 16, 2, DtypeMap::Mode::I16 };
        case ValueType::INT32:   return { 4, 32, 2, DtypeMap::Mode::I32 };
        case ValueType::SINGLE:  return { 4, 32, 3, DtypeMap::Mode::F32 };
        case ValueType::DOUBLE:  return { 8, 64, 3, DtypeMap::Mode::F64 };
        default:
            throw Error("imwrite TIFF: unsupported input type",
                        0, 0, "imwrite", "", "numkit:imwrite:tiffType");
    }
}

// Encode pixels in row-major chunky layout, little-endian within each
// sample. The destination byte count is H · W · S · bytesPerSample.
std::vector<std::uint8_t>
extractChunkyBytes(const Value &A, std::size_t H, std::size_t W, std::size_t S,
                   const DtypeMap &dm)
{
    const std::size_t bps = dm.bytesPerSample;
    const std::size_t plane = H * W;
    std::vector<std::uint8_t> bytes(H * W * S * bps);

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

// Apply horizontal differencing predictor in-place: for each row,
// replace pixel[i, s] with pixel[i, s] - pixel[i-1, s] (per sample
// component for chunky layout, first pixel unchanged). Used to improve
// LZW / Deflate compression ratios — must set Predictor tag (317) = 2
// when applied. MATLAB's `imwrite(..., 'tif', 'Compression', 'lzw')`
// applies this by default.
void applyHorizontalDiff(std::vector<std::uint8_t> &buf, std::size_t H,
                          std::size_t W, std::size_t S, std::size_t bps)
{
    const std::size_t rowBytes = W * S * bps;
    if (bps == 1) {
        for (std::size_t r = 0; r < H; ++r) {
            std::uint8_t *row = buf.data() + r * rowBytes;
            // Walk RTL so each diff sees the original prev pixel.
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
    // bps==8 (DOUBLE) is supported only without predictor (predictor=2
    // is undefined for floating-point widths > 4 in the TIFF spec).
}

// Resolve the compression code from a string ('none'|'packbits'|'lzw'|'deflate').
std::uint16_t parseCompression(const std::string &s)
{
    if (s.empty() || s == "none")     return 1;
    if (s == "packbits")              return 32773;
    if (s == "lzw")                   return 5;
    if (s == "deflate")               return 8;
    throw Error("imwrite TIFF: unknown Compression '" + s + "' (use none, packbits, lzw, or deflate)",
                0, 0, "imwrite", "", "numkit:imwrite:tiffCompression");
}

// Compress one strip's worth of bytes according to `compression`.
std::vector<std::uint8_t>
compressStrip(const std::uint8_t *src, std::size_t n, std::uint16_t compression)
{
    switch (compression) {
        case 1:     return std::vector<std::uint8_t>(src, src + n);
        case 32773: return encodePackBits(src, n);
        case 5:     return encodeLZW(src, n);
        case 8:
#ifdef NUMKIT_WITH_ZLIB
            return encodeDeflate(src, n);
#else
            throw Error("imwrite TIFF: Deflate requires zlib at build time",
                        0, 0, "imwrite", "", "numkit:imwrite:tiffDeflate");
#endif
        default:
            throw Error("imwrite TIFF: unsupported compression",
                        0, 0, "imwrite", "", "numkit:imwrite:tiffCompression");
    }
}

// ── core page writer ───────────────────────────────────────────────

// Write one IFD's worth of bytes into `buf` (file image being built).
// `buf` is the entire file under construction; pixel data + IFD are
// appended at its end.
//
// For multi-page append: the caller passes the offset of the previous
// IFD's "next-IFD" slot via `prevNextIFDOff` (or SIZE_MAX for single-
// page / first page). After writing this IFD, that slot is patched
// to point at the new IFD.
void writePage(std::vector<std::uint8_t> &buf, const Value &A,
               std::uint16_t compression,
               std::size_t prevNextIFDOff)
{
    // Resolve image shape from the Value (column-major).
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

    // Horizontal predictor — improves LZW / Deflate compression and
    // matches MATLAB's default `imwrite(..., 'tif', 'Compression', 'lzw')`
    // output. TIFF spec restricts predictor=2 to 8/16/32-bit integer
    // samples (not float). Apply only when both apply.
    const bool useHPred = (compression == 5 || compression == 8 || compression == 32946)
                          && (dm.sampleFormat != 3)
                          && (bps == 1 || bps == 2 || bps == 4);
    if (useHPred)
        applyHorizontalDiff(bytes, H, W, S, bps);
    const std::uint16_t predictor = useHPred ? 2u : 1u;

    auto strip = compressStrip(bytes.data(), bytes.size(), compression);

    // Pad to even offset for tag alignment (TIFF convention recommends).
    if ((buf.size() & 1) != 0) buf.push_back(0);
    const std::uint32_t stripOffset = static_cast<std::uint32_t>(buf.size());
    buf.insert(buf.end(), strip.begin(), strip.end());

    // Pad to even — BitsPerSample array for multi-sample images needs
    // its own offset; lay it out BEFORE the IFD so the IFD body, which
    // overwrites buf[ifdOffset..ifdEnd), doesn't clobber it.
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

    // Build the IFD. Entries are emitted in ascending tag order
    // (TIFF 6.0 spec requirement).
    struct Entry {
        std::uint16_t tag, type;
        std::uint32_t count;
        std::uint32_t value;  // inline if size ≤ 4 bytes, otherwise offset
    };
    std::vector<Entry> entries;

    // Build entries (ascending tag order).
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

    // IFD layout: 2-byte count + 12-byte entries + 4-byte next-IFD offset.
    const std::size_t ifdBytes = 2 + entries.size() * 12 + 4;
    const std::size_t ifdEnd = ifdOffset + ifdBytes;
    buf.resize(ifdEnd, 0);

    writeU16LE(buf, ifdOffset, static_cast<std::uint16_t>(entries.size()));
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const std::size_t e = ifdOffset + 2 + i * 12;
        writeU16LE(buf, e + 0, entries[i].tag);
        writeU16LE(buf, e + 2, entries[i].type);
        writeU32LE(buf, e + 4, entries[i].count);
        // Inline encoding for SHORT count==1 — value goes into low bytes,
        // high bytes stay zero. Same for our other small entries.
        writeU32LE(buf, e + 8, entries[i].value);
    }
    // Next-IFD offset slot at ifdEnd - 4 — leave 0 for now (last page).
    writeU32LE(buf, ifdEnd - 4, 0u);

    // Patch the previous page's next-IFD slot to point at this IFD.
    if (prevNextIFDOff != SIZE_MAX)
        writeU32LE(buf, prevNextIFDOff, ifdOffset);
}

} // anonymous

std::vector<std::uint8_t>
writeTiffToBytes(const Value &A, const std::string &compression,
                 const std::vector<std::uint8_t> *existing)
{
    const std::uint16_t comp = parseCompression(compression);
    std::vector<std::uint8_t> buf;

    if (existing && !existing->empty()) {
        // Multi-page append: start from the existing bytes, walk to the
        // LAST IFD, then write a new IFD whose offset is patched into that
        // last IFD's next-IFD slot.
        buf = *existing;
        if (buf.size() < 8 || !(buf[0] == 'I' && buf[1] == 'I')
            || buf[2] != 0x2A || buf[3] != 0x00)
            throw Error("imwrite TIFF: append target is not a little-endian TIFF",
                        0, 0, "imwrite", "", "numkit:imwrite:tiffMagic");
        // Walk IFD chain to find the LAST page's next-IFD slot.
        std::uint32_t off = static_cast<std::uint32_t>(
            buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24));
        std::size_t lastNextSlot = 4;  // default: first-IFD slot at offset 4
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
        // Fresh write: 8-byte header (II + 42 + 0 first-IFD offset placeholder),
        // then page (writes pixel strip + IFD), then patch header's IFD offset.
        buf.assign(8, 0);
        buf[0] = 'I'; buf[1] = 'I';
        writeU16LE(buf, 2, 42);
        writeU32LE(buf, 4, 0);  // placeholder
        writePage(buf, A, comp, /*prevNextIFDOff=*/4);
    }
    return buf;
}

void writeTiff(const Value &A, const std::string &path,
               const std::string &compression,
               bool appendMode)
{
    // Read the existing file (append mode) so writeTiffToBytes can chain a
    // new IFD onto it, then persist the assembled bytes to disk.
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
