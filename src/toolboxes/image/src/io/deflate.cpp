// toolboxes/image/src/io/deflate.cpp
//
// In-tree RFC 1951 Deflate/Inflate, RFC 1950 ZLIB wrapper, and
// CRC-32 / Adler-32 checksums. Zero external dependencies.

#include "deflate.hpp"
#include <numkit/value/error.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace numkit::image {

// ============================================================================
// 1. Checksums: CRC-32 and Adler-32
// ============================================================================

namespace {

// Compile-time CRC-32 table (polynomial 0xEDB88320).
constexpr auto makeCrc32Table() {
    std::array<std::uint32_t, 256> tbl{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        tbl[i] = c;
    }
    return tbl;
}

constexpr auto kCrc32Table = makeCrc32Table();

} // anonymous namespace

std::uint32_t crc32(const std::uint8_t *data, std::size_t len) {
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        c = kCrc32Table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

std::uint32_t adler32(const std::uint8_t *data, std::size_t len) {
    constexpr std::uint32_t kBase = 65521u;
    std::uint32_t s1 = 1u;
    std::uint32_t s2 = 0u;

    while (len > 0) {
        std::size_t k = (len < 5552) ? len : 5552;
        len -= k;
        while (k >= 16) {
            s1 += data[0]; s2 += s1;
            s1 += data[1]; s2 += s1;
            s1 += data[2]; s2 += s1;
            s1 += data[3]; s2 += s1;
            s1 += data[4]; s2 += s1;
            s1 += data[5]; s2 += s1;
            s1 += data[6]; s2 += s1;
            s1 += data[7]; s2 += s1;
            s1 += data[8]; s2 += s1;
            s1 += data[9]; s2 += s1;
            s1 += data[10]; s2 += s1;
            s1 += data[11]; s2 += s1;
            s1 += data[12]; s2 += s1;
            s1 += data[13]; s2 += s1;
            s1 += data[14]; s2 += s1;
            s1 += data[15]; s2 += s1;
            data += 16;
            k -= 16;
        }
        while (k > 0) {
            s1 += *data++;
            s2 += s1;
            --k;
        }
        s1 %= kBase;
        s2 %= kBase;
    }
    return (s2 << 16) | s1;
}

// ============================================================================
// 2. RFC 1951 Constants and Huffman Table Structure
// ============================================================================

namespace {

constexpr std::uint16_t kLenBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

constexpr std::uint8_t kLenExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

constexpr std::uint16_t kDistBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

constexpr std::uint8_t kDistExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

constexpr std::uint8_t kClenOrder[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

// Fast LSB-first Bit Stream Reader
class BitReader {
public:
    BitReader(const std::uint8_t *data, std::size_t size)
        : ptr_(data), end_(data + size), bitBuf_(0), bitCount_(0) {}

    void ensureBits(int n) {
        while (bitCount_ < n) {
            if (ptr_ >= end_) return; // out of input
            bitBuf_ |= static_cast<std::uint64_t>(*ptr_++) << bitCount_;
            bitCount_ += 8;
        }
    }

    std::uint32_t peekBits(int n) {
        ensureBits(n);
        return static_cast<std::uint32_t>(bitBuf_ & ((1ull << n) - 1));
    }

    void dropBits(int n) {
        bitBuf_ >>= n;
        bitCount_ -= n;
    }

    std::uint32_t readBits(int n) {
        if (n == 0) return 0;
        ensureBits(n);
        if (bitCount_ < n) {
            throw Error("deflate: unexpected end of stream",
                        0, 0, "inflateRaw", "", "numkit:inflate:eof");
        }
        std::uint32_t val = static_cast<std::uint32_t>(bitBuf_ & ((1ull << n) - 1));
        dropBits(n);
        return val;
    }

    void alignToByte() {
        int drop = bitCount_ % 8;
        if (drop != 0) dropBits(drop);
    }

    const std::uint8_t *currentBytePtr() {
        alignToByte();
        // Undo lookahead in bitBuf_
        return ptr_ - (bitCount_ / 8);
    }

    void resetBytePtr(const std::uint8_t *p) {
        ptr_ = p;
        bitBuf_ = 0;
        bitCount_ = 0;
    }

    std::size_t remainingBytes() const {
        std::size_t unreadInBuf = static_cast<std::size_t>(bitCount_ / 8);
        return (end_ >= ptr_) ? (static_cast<std::size_t>(end_ - ptr_) + unreadInBuf) : 0;
    }

private:
    const std::uint8_t *ptr_;
    const std::uint8_t *end_;
    std::uint64_t bitBuf_;
    int bitCount_;
};

// Fast Huffman Decoder using primary direct table + secondary overflow list
class HuffmanTable {
public:
    static constexpr int kFastBits = 9;
    static constexpr int kFastSize = 1 << kFastBits;

    void build(const std::uint8_t *lens, std::size_t numSymbols) {
        std::array<int, 16> count{};
        for (std::size_t i = 0; i < numSymbols; ++i) {
            if (lens[i] > 15) throw Error("deflate: invalid code length > 15");
            if (lens[i] > 0) count[lens[i]]++;
        }

        std::array<int, 16> nextCode{};
        int code = 0;
        count[0] = 0;
        for (int bits = 1; bits <= 15; ++bits) {
            code = (code + count[bits - 1]) << 1;
            nextCode[bits] = code;
        }

        // Fast table entry: bits 0..7 symbol (or offset), bits 8..11 code length, bit 15 secondary flag
        fastTable_.fill(0);
        overflowEntries_.clear();

        for (std::size_t sym = 0; sym < numSymbols; ++sym) {
            int len = lens[sym];
            if (len == 0) continue;

            int c = nextCode[len]++;
            // Bit-reverse code of length `len`
            int rev = 0;
            for (int j = 0; j < len; ++j) {
                rev = (rev << 1) | ((c >> j) & 1);
            }

            if (len <= kFastBits) {
                std::uint16_t entry = static_cast<std::uint16_t>((len << 9) | sym);
                int step = 1 << len;
                for (int m = rev; m < kFastSize; m += step) {
                    fastTable_[m] = entry;
                }
            } else {
                // Secondary overflow entry
                overflowEntries_.push_back(OverflowEntry{
                    static_cast<std::uint16_t>(sym),
                    static_cast<std::uint8_t>(len),
                    static_cast<std::uint16_t>(rev)
                });
            }
        }

        // Mark fast table entries that lead to overflow
        if (!overflowEntries_.empty()) {
            for (const auto &oe : overflowEntries_) {
                int prefix = oe.rev & (kFastSize - 1);
                fastTable_[prefix] = 0x8000; // Flag as overflow
            }
        }
    }

    std::uint32_t decode(BitReader &reader) const {
        reader.ensureBits(kFastBits);
        std::uint32_t peek = reader.peekBits(kFastBits);
        std::uint16_t entry = fastTable_[peek];

        if ((entry & 0x8000) == 0 && entry != 0) {
            int len = (entry >> 9) & 0x0F;
            reader.dropBits(len);
            return entry & 0x1FF;
        }

        // Slow path: match against overflow list or sequential lookup
        reader.ensureBits(15);
        std::uint32_t fullPeek = reader.peekBits(15);
        for (const auto &oe : overflowEntries_) {
            if ((fullPeek & ((1u << oe.len) - 1)) == oe.rev) {
                reader.dropBits(oe.len);
                return oe.symbol;
            }
        }

        throw Error("deflate: invalid Huffman code in bitstream",
                    0, 0, "inflateRaw", "", "numkit:inflate:badCode");
    }

private:
    struct OverflowEntry {
        std::uint16_t symbol;
        std::uint8_t len;
        std::uint16_t rev;
    };
    std::array<std::uint16_t, kFastSize> fastTable_{};
    std::vector<OverflowEntry> overflowEntries_;
};

// Singleton Fixed Huffman Tables
const HuffmanTable &getFixedLitTable() {
    static const HuffmanTable table = []() {
        std::array<std::uint8_t, 288> lens{};
        for (int i = 0; i <= 143; ++i) lens[i] = 8;
        for (int i = 144; i <= 255; ++i) lens[i] = 9;
        for (int i = 256; i <= 279; ++i) lens[i] = 7;
        for (int i = 280; i <= 287; ++i) lens[i] = 8;
        HuffmanTable t;
        t.build(lens.data(), lens.size());
        return t;
    }();
    return table;
}

const HuffmanTable &getFixedDistTable() {
    static const HuffmanTable table = []() {
        std::array<std::uint8_t, 32> lens{};
        lens.fill(5);
        HuffmanTable t;
        t.build(lens.data(), lens.size());
        return t;
    }();
    return table;
}

} // anonymous namespace

// ============================================================================
// 3. RFC 1951 Inflate (Decompress)
// ============================================================================

std::vector<std::uint8_t> inflateRaw(const std::uint8_t *src, std::size_t len,
                                    std::size_t expectedSizeHint)
{
    BitReader reader(src, len);
    std::vector<std::uint8_t> out;
    if (expectedSizeHint > 0) out.reserve(expectedSizeHint);

    bool bfinal = false;
    while (!bfinal) {
        bfinal = (reader.readBits(1) != 0);
        std::uint32_t btype = reader.readBits(2);

        if (btype == 0) {
            // Stored / Uncompressed block
            reader.alignToByte();
            const std::uint8_t *rawPtr = reader.currentBytePtr();
            std::size_t rem = reader.remainingBytes();
            if (rem < 4) {
                throw Error("deflate: truncated stored block header",
                            0, 0, "inflateRaw", "", "numkit:inflate:storedEOF");
            }
            std::uint16_t blen = static_cast<std::uint16_t>(rawPtr[0] | (rawPtr[1] << 8));
            std::uint16_t nlen = static_cast<std::uint16_t>(rawPtr[2] | (rawPtr[3] << 8));
            if (static_cast<std::uint16_t>(blen ^ 0xFFFF) != nlen) {
                throw Error("deflate: stored block length mismatch",
                            0, 0, "inflateRaw", "", "numkit:inflate:storedLen");
            }
            rawPtr += 4;
            rem -= 4;
            if (rem < blen) {
                throw Error("deflate: truncated stored block data",
                            0, 0, "inflateRaw", "", "numkit:inflate:storedData");
            }
            out.insert(out.end(), rawPtr, rawPtr + blen);
            reader.resetBytePtr(rawPtr + blen);
        } else if (btype == 1 || btype == 2) {
            // Compressed block: 1 = Fixed Huffman, 2 = Dynamic Huffman
            HuffmanTable dynLitTable;
            HuffmanTable dynDistTable;
            const HuffmanTable *litTable = nullptr;
            const HuffmanTable *distTable = nullptr;

            if (btype == 1) {
                litTable = &getFixedLitTable();
                distTable = &getFixedDistTable();
            } else {
                std::uint32_t hlit = reader.readBits(5) + 257;
                std::uint32_t hdist = reader.readBits(5) + 1;
                std::uint32_t hclen = reader.readBits(4) + 4;

                if (hlit > 286 || hdist > 30) {
                    throw Error("deflate: invalid dynamic block header counts",
                                0, 0, "inflateRaw", "", "numkit:inflate:dynHeader");
                }

                std::array<std::uint8_t, 19> clenLens{};
                for (std::size_t i = 0; i < hclen; ++i) {
                    clenLens[kClenOrder[i]] = static_cast<std::uint8_t>(reader.readBits(3));
                }

                HuffmanTable clenTable;
                clenTable.build(clenLens.data(), clenLens.size());

                std::vector<std::uint8_t> allLens(hlit + hdist, 0);
                std::size_t index = 0;
                while (index < hlit + hdist) {
                    std::uint32_t sym = clenTable.decode(reader);
                    if (sym < 16) {
                        allLens[index++] = static_cast<std::uint8_t>(sym);
                    } else if (sym == 16) {
                        if (index == 0) throw Error("deflate: repeat with no previous code");
                        std::uint8_t prev = allLens[index - 1];
                        std::size_t rep = reader.readBits(2) + 3;
                        while (rep-- > 0 && index < hlit + hdist) allLens[index++] = prev;
                    } else if (sym == 17) {
                        std::size_t rep = reader.readBits(3) + 3;
                        while (rep-- > 0 && index < hlit + hdist) allLens[index++] = 0;
                    } else if (sym == 18) {
                        std::size_t rep = reader.readBits(7) + 11;
                        while (rep-- > 0 && index < hlit + hdist) allLens[index++] = 0;
                    }
                }

                dynLitTable.build(allLens.data(), hlit);
                dynDistTable.build(allLens.data() + hlit, hdist);
                litTable = &dynLitTable;
                distTable = &dynDistTable;
            }

            // Decode literal/length stream
            while (true) {
                std::uint32_t sym = litTable->decode(reader);
                if (sym < 256) {
                    out.push_back(static_cast<std::uint8_t>(sym));
                } else if (sym == 256) {
                    break; // End of block
                } else {
                    std::uint32_t lenIdx = sym - 257;
                    if (lenIdx >= 29) {
                        throw Error("deflate: invalid length code",
                                    0, 0, "inflateRaw", "", "numkit:inflate:badLen");
                    }
                    std::size_t matchLen = kLenBase[lenIdx] + reader.readBits(kLenExtra[lenIdx]);
                    std::uint32_t distCode = distTable->decode(reader);
                    if (distCode >= 30) {
                        throw Error("deflate: invalid distance code",
                                    0, 0, "inflateRaw", "", "numkit:inflate:badDist");
                    }
                    std::size_t matchDist = kDistBase[distCode] + reader.readBits(kDistExtra[distCode]);

                    if (matchDist > out.size()) {
                        throw Error("deflate: distance exceeds output buffer (backward copy OOB)",
                                    0, 0, "inflateRaw", "", "numkit:inflate:distOOB");
                    }

                    // Copy bytes from history
                    std::size_t srcPos = out.size() - matchDist;
                    for (std::size_t m = 0; m < matchLen; ++m) {
                        out.push_back(out[srcPos + m]);
                    }
                }
            }
        } else {
            throw Error("deflate: reserved block type 3",
                        0, 0, "inflateRaw", "", "numkit:inflate:badType");
        }
    }

    return out;
}

// ============================================================================
// 4. RFC 1951 Deflate (Compress)
// ============================================================================

namespace {

class BitWriter {
public:
    void writeBits(std::uint32_t val, int n) {
        bitBuf_ |= (static_cast<std::uint64_t>(val) & ((1ull << n) - 1)) << bitCount_;
        bitCount_ += n;
        while (bitCount_ >= 8) {
            buf_.push_back(static_cast<std::uint8_t>(bitBuf_ & 0xFF));
            bitBuf_ >>= 8;
            bitCount_ -= 8;
        }
    }

    void flushBits() {
        if (bitCount_ > 0) {
            buf_.push_back(static_cast<std::uint8_t>(bitBuf_ & 0xFF));
            bitBuf_ = 0;
            bitCount_ = 0;
        }
    }

    std::vector<std::uint8_t> finish() {
        flushBits();
        return std::move(buf_);
    }

private:
    std::vector<std::uint8_t> buf_;
    std::uint64_t bitBuf_ = 0;
    int bitCount_ = 0;
};

// Fixed Huffman code values and lengths for Literals 0..287
struct FixedHuffCodes {
    std::array<std::uint16_t, 288> codes{};
    std::array<std::uint8_t, 288> lens{};

    constexpr FixedHuffCodes() {
        // 0..143: 8 bits (00110000 .. 10111111) -> 0x30 + i
        for (int i = 0; i <= 143; ++i) {
            lens[i] = 8;
            int c = 0x30 + i;
            int rev = 0;
            for (int b = 0; b < 8; ++b) rev = (rev << 1) | ((c >> b) & 1);
            codes[i] = static_cast<std::uint16_t>(rev);
        }
        // 144..255: 9 bits (110010000 .. 111111111) -> 0x190 + (i - 144)
        for (int i = 144; i <= 255; ++i) {
            lens[i] = 9;
            int c = 0x190 + (i - 144);
            int rev = 0;
            for (int b = 0; b < 9; ++b) rev = (rev << 1) | ((c >> b) & 1);
            codes[i] = static_cast<std::uint16_t>(rev);
        }
        // 256..279: 7 bits (0000000 .. 0010111) -> (i - 256)
        for (int i = 256; i <= 279; ++i) {
            lens[i] = 7;
            int c = i - 256;
            int rev = 0;
            for (int b = 0; b < 7; ++b) rev = (rev << 1) | ((c >> b) & 1);
            codes[i] = static_cast<std::uint16_t>(rev);
        }
        // 280..287: 8 bits (11000000 .. 11000111) -> 0xC0 + (i - 280)
        for (int i = 280; i <= 287; ++i) {
            lens[i] = 8;
            int c = 0xC0 + (i - 280);
            int rev = 0;
            for (int b = 0; b < 8; ++b) rev = (rev << 1) | ((c >> b) & 1);
            codes[i] = static_cast<std::uint16_t>(rev);
        }
    }
};

constexpr FixedHuffCodes kFixedCodes;

} // anonymous namespace

std::vector<std::uint8_t> deflateRaw(const std::uint8_t *src, std::size_t len, int level) {
    if (len == 0) {
        // Empty block with BFINAL=1, BTYPE=01 (fixed), EOB (256)
        BitWriter writer;
        writer.writeBits(1, 1); // BFINAL=1
        writer.writeBits(1, 2); // BTYPE=01 (Fixed Huffman)
        writer.writeBits(kFixedCodes.codes[256], kFixedCodes.lens[256]);
        return writer.finish();
    }

    if (level <= 0) {
        // Stored uncompressed block(s)
        std::vector<std::uint8_t> out;
        std::size_t offset = 0;
        while (offset < len) {
            std::size_t chunk = std::min<std::size_t>(len - offset, 65535);
            bool last = (offset + chunk >= len);
            out.push_back(last ? 1 : 0); // BFINAL + BTYPE=00
            std::uint16_t blen = static_cast<std::uint16_t>(chunk);
            std::uint16_t nlen = static_cast<std::uint16_t>(~blen);
            out.push_back(static_cast<std::uint8_t>(blen & 0xFF));
            out.push_back(static_cast<std::uint8_t>((blen >> 8) & 0xFF));
            out.push_back(static_cast<std::uint8_t>(nlen & 0xFF));
            out.push_back(static_cast<std::uint8_t>((nlen >> 8) & 0xFF));
            out.insert(out.end(), src + offset, src + offset + chunk);
            offset += chunk;
        }
        return out;
    }

    // Level 1..9: LZ77 match search with Fixed Huffman encoding
    BitWriter writer;
    writer.writeBits(1, 1); // BFINAL=1
    writer.writeBits(1, 2); // BTYPE=01 (Fixed Huffman)

    constexpr std::size_t kHashSize = 32768;
    constexpr std::size_t kWindowSize = 32768;
    std::vector<int> head(kHashSize, -1);
    std::vector<int> prev(len, -1);

    auto calcHash3 = [](const std::uint8_t *p) -> std::size_t {
        return ((static_cast<std::size_t>(p[0]) << 10) ^
                (static_cast<std::size_t>(p[1]) << 5)  ^
                static_cast<std::size_t>(p[2])) & 32767u;
    };

    std::size_t i = 0;
    while (i < len) {
        int matchLen = 0;
        int matchDist = 0;

        if (i + 3 <= len) {
            std::size_t h = calcHash3(src + i);
            int matchPos = head[h];
            prev[i] = matchPos;
            head[h] = static_cast<int>(i);

            int chainLimit = (level >= 6) ? 32 : 16;
            while (matchPos != -1 && (static_cast<int>(i) - matchPos) <= static_cast<int>(kWindowSize) && chainLimit-- > 0) {
                int l = 0;
                int maxL = static_cast<int>(std::min<std::size_t>(len - i, 258));
                while (l < maxL && src[i + l] == src[matchPos + l]) {
                    ++l;
                }
                if (l > matchLen) {
                    matchLen = l;
                    matchDist = static_cast<int>(i) - matchPos;
                    if (matchLen >= 258) break;
                }
                matchPos = prev[matchPos];
            }
        }

        if (matchLen >= 3) {
            // Find length code
            int lenCode = 0;
            while (lenCode < 28 && kLenBase[lenCode + 1] <= matchLen) ++lenCode;
            int litSym = 257 + lenCode;
            writer.writeBits(kFixedCodes.codes[litSym], kFixedCodes.lens[litSym]);
            int extraLenBits = kLenExtra[lenCode];
            if (extraLenBits > 0) {
                writer.writeBits(matchLen - kLenBase[lenCode], extraLenBits);
            }

            // Find distance code
            int distCode = 0;
            while (distCode < 29 && kDistBase[distCode + 1] <= matchDist) ++distCode;
            // Fixed distance code is 5 bits reversed
            int distRev = 0;
            for (int b = 0; b < 5; ++b) distRev = (distRev << 1) | ((distCode >> b) & 1);
            writer.writeBits(distRev, 5);
            int extraDistBits = kDistExtra[distCode];
            if (extraDistBits > 0) {
                writer.writeBits(matchDist - kDistBase[distCode], extraDistBits);
            }

            // Update hash table for skipped bytes
            for (int k = 1; k < matchLen; ++k) {
                if (i + k + 2 < len) {
                    std::size_t hk = calcHash3(src + i + k);
                    prev[i + k] = head[hk];
                    head[hk] = static_cast<int>(i + k);
                }
            }
            i += matchLen;
        } else {
            // Literal byte
            std::uint8_t b = src[i++];
            writer.writeBits(kFixedCodes.codes[b], kFixedCodes.lens[b]);
        }
    }

    // End of block symbol (256)
    writer.writeBits(kFixedCodes.codes[256], kFixedCodes.lens[256]);
    return writer.finish();
}

// ============================================================================
// 5. RFC 1950 ZLIB Decompress & Compress Wrappers
// ============================================================================

std::vector<std::uint8_t> zlibDecompress(const std::uint8_t *src, std::size_t len,
                                         std::size_t expectedSizeHint)
{
    if (len < 6) {
        throw Error("zlib: stream too short (< 6 bytes)",
                    0, 0, "zlibDecompress", "", "numkit:zlib:truncated");
    }

    std::uint8_t cmf = src[0];
    std::uint8_t flg = src[1];

    if ((static_cast<std::uint32_t>(cmf) * 256 + flg) % 31 != 0) {
        throw Error("zlib: invalid header checksum",
                    0, 0, "zlibDecompress", "", "numkit:zlib:badHeader");
    }
    if ((cmf & 0x0F) != 8) {
        throw Error("zlib: unsupported compression method (only Deflate supported)",
                    0, 0, "zlibDecompress", "", "numkit:zlib:badMethod");
    }
    if (flg & 0x20) {
        throw Error("zlib: preset dictionaries not supported",
                    0, 0, "zlibDecompress", "", "numkit:zlib:fdict");
    }

    // Decompress payload
    std::vector<std::uint8_t> out = inflateRaw(src + 2, len - 6, expectedSizeHint);

    // Verify 4-byte big-endian Adler-32
    std::uint32_t expectedAdler = (static_cast<std::uint32_t>(src[len - 4]) << 24) |
                                  (static_cast<std::uint32_t>(src[len - 3]) << 16) |
                                  (static_cast<std::uint32_t>(src[len - 2]) << 8)  |
                                  static_cast<std::uint32_t>(src[len - 1]);

    std::uint32_t actualAdler = adler32(out.data(), out.size());
    if (actualAdler != expectedAdler) {
        throw Error("zlib: Adler-32 checksum mismatch",
                    0, 0, "zlibDecompress", "", "numkit:zlib:adlerMismatch");
    }

    return out;
}

std::vector<std::uint8_t> zlibCompress(const std::uint8_t *src, std::size_t len, int level) {
    std::vector<std::uint8_t> compressed = deflateRaw(src, len, level);

    std::vector<std::uint8_t> out;
    out.reserve(2 + compressed.size() + 4);

    // Zlib header: CMF = 0x78 (Deflate, 32K window), FLG = 0x01 (level check)
    // (0x78 * 256 + 0x01) % 31 == 0
    std::uint8_t cmf = 0x78;
    std::uint8_t flg = 0x01;
    out.push_back(cmf);
    out.push_back(flg);

    out.insert(out.end(), compressed.begin(), compressed.end());

    // 4-byte big-endian Adler-32 checksum
    std::uint32_t chk = adler32(src, len);
    out.push_back(static_cast<std::uint8_t>((chk >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((chk >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((chk >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(chk & 0xFF));

    return out;
}

} // namespace numkit::image
