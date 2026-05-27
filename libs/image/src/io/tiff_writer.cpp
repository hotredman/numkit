// libs/image/src/io/tiff_writer.cpp
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

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

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
            std::size_t h = 1469598103934665603ull;
            for (auto b : v) {
                h ^= b;
                h *= 1099511628211ull;
            }
            return h;
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

// Encode the image's pixels into row-major chunky uint8/uint16 bytes,
// LE within each sample. Throws for unsupported dtypes.
std::vector<std::uint8_t>
extractChunkyBytes(const Value &A, std::size_t H, std::size_t W, std::size_t S,
                   std::size_t &bps_out)
{
    const ValueType vt = A.type();
    std::size_t bps;
    bool isU16 = false;
    switch (vt) {
        case ValueType::UINT8:
        case ValueType::LOGICAL:
        case ValueType::CHAR:
        case ValueType::DOUBLE:  // clamp to 0..255
        case ValueType::SINGLE:
            bps = 1; break;
        case ValueType::UINT16:
            bps = 2; isU16 = true; break;
        default:
            throw Error("imwrite TIFF: unsupported input type (need uint8/uint16 or numeric → uint8)",
                        0, 0, "imwrite", "", "numkit:imwrite:tiffType");
    }
    bps_out = bps;
    const std::size_t plane = H * W;
    std::vector<std::uint8_t> bytes(H * W * S * bps);
    for (std::size_t r = 0; r < H; ++r)
        for (std::size_t c = 0; c < W; ++c)
            for (std::size_t s = 0; s < S; ++s) {
                const std::size_t srcIdx = (S == 1)
                    ? (c * H + r)
                    : (s * plane + c * H + r);
                const std::size_t dstByte = (r * W + c) * S * bps + s * bps;
                if (isU16) {
                    const std::uint16_t v = A.uint16Data()[srcIdx];
                    bytes[dstByte]     = static_cast<std::uint8_t>(v & 0xFF);
                    bytes[dstByte + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
                } else {
                    double dv = A.elemAsDouble(srcIdx);
                    if (dv < 0) dv = 0;
                    if (dv > 255) dv = 255;
                    bytes[dstByte] = static_cast<std::uint8_t>(static_cast<int>(dv));
                }
            }
    return bytes;
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

    std::size_t bps = 0;
    auto bytes = extractChunkyBytes(A, H, W, S, bps);
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
    const std::uint16_t bitsPerSample = static_cast<std::uint16_t>(bps * 8);

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
    entries.push_back({284, 3, 1, 1u});  // PlanarConfig chunky
    entries.push_back({339, 3, 1, 1u});  // SampleFormat = unsigned int

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

void writeTiff(const Value &A, const std::string &path,
               const std::string &compression,
               bool appendMode)
{
    const std::uint16_t comp = parseCompression(compression);

    if (appendMode && std::filesystem::exists(path)) {
        // Multi-page append: load the existing file, walk to the LAST
        // IFD, then write a new IFD whose offset is patched into that
        // last IFD's next-IFD slot.
        std::ifstream in(path, std::ios::binary);
        if (!in) throw Error("imwrite TIFF: cannot reopen file for append",
                              0, 0, "imwrite", "", "numkit:imwrite:open");
        in.seekg(0, std::ios::end);
        const std::streamoff sz = in.tellg();
        in.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
        in.read(reinterpret_cast<char *>(buf.data()), sz);
        in.close();

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

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) throw Error("imwrite TIFF: cannot reopen for write",
                               0, 0, "imwrite", "", "numkit:imwrite:open");
        out.write(reinterpret_cast<const char *>(buf.data()), buf.size());
    } else {
        // Fresh write: 8-byte header (II + 42 + 0 first-IFD offset placeholder),
        // then page (writes pixel strip + IFD), then patch header's IFD offset.
        std::vector<std::uint8_t> buf(8, 0);
        buf[0] = 'I'; buf[1] = 'I';
        writeU16LE(buf, 2, 42);
        writeU32LE(buf, 4, 0);  // placeholder

        // Write the page, passing offset 4 so the header's first-IFD slot
        // gets patched.
        writePage(buf, A, comp, /*prevNextIFDOff=*/4);

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) throw Error("imwrite TIFF: cannot open '" + path + "' for write",
                               0, 0, "imwrite", "", "numkit:imwrite:open");
        out.write(reinterpret_cast<const char *>(buf.data()), buf.size());
    }
}

} // namespace numkit::image
