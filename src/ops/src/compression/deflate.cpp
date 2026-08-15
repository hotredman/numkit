// ops/src/compression/deflate.cpp
//
// In-tree RFC 1951 Deflate/Inflate, RFC 1950 ZLIB wrapper, and
// CRC-32 / Adler-32 checksums. Zero external dependencies.

#include <numkit/ops/deflate.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace numkit::ops {

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
// 2. Bitstream & Huffman Trees
// ============================================================================

namespace {

class BitReader {
public:
    BitReader(const std::uint8_t *src, std::size_t len)
        : data_(src), len_(len), pos_(0), bitBuf_(0), bitCount_(0) {}

    std::uint32_t readBits(int n) {
        while (bitCount_ < n) {
            if (pos_ >= len_) {
                throw Error("deflate: unexpected end of stream",
                            0, 0, "inflateRaw", "", "numkit:deflate:truncated");
            }
            bitBuf_ |= (static_cast<std::uint32_t>(data_[pos_++]) << bitCount_);
            bitCount_ += 8;
        }
        std::uint32_t val = bitBuf_ & ((1u << n) - 1u);
        bitBuf_ >>= n;
        bitCount_ -= n;
        return val;
    }

    void alignToByte() {
        bitBuf_ = 0;
        bitCount_ = 0;
    }

    std::uint8_t readByte() {
        if (bitCount_ > 0) alignToByte();
        if (pos_ >= len_) {
            throw Error("deflate: unexpected end of stream",
                        0, 0, "inflateRaw", "", "numkit:deflate:truncated");
        }
        return data_[pos_++];
    }

    const std::uint8_t *currentPtr() const { return data_ + pos_; }
    std::size_t remainingBytes() const { return (pos_ <= len_) ? (len_ - pos_) : 0; }

private:
    const std::uint8_t *data_;
    std::size_t len_;
    std::size_t pos_;
    std::uint32_t bitBuf_;
    int bitCount_;
};

struct HuffmanTree {
    std::vector<std::int16_t> tree; // left: 2*node, right: 2*node+1, leaf: -symbol-1

    void build(const std::uint8_t *lens, int count) {
        tree.assign(2, 0); // root node = 0 (left child = 0, right child = 0)

        // Find max bit length
        int maxLen = 0;
        for (int i = 0; i < count; ++i) {
            if (lens[i] > maxLen) maxLen = lens[i];
        }
        if (maxLen == 0) return;

        // Count code lengths
        std::vector<int> bl_count(maxLen + 1, 0);
        for (int i = 0; i < count; ++i) {
            if (lens[i] > 0) bl_count[lens[i]]++;
        }

        // Compute base codes
        std::vector<int> next_code(maxLen + 1, 0);
        int code = 0;
        for (int bits = 1; bits <= maxLen; ++bits) {
            code = (code + bl_count[bits - 1]) << 1;
            next_code[bits] = code;
        }

        // Insert symbols into binary trie
        for (int i = 0; i < count; ++i) {
            int len = lens[i];
            if (len == 0) continue;
            int c = next_code[len]++;
            int node = 0;
            for (int b = len - 1; b >= 0; --b) {
                int bit = (c >> b) & 1;
                int childIdx = (node * 2) + bit;
                if (b == 0) {
                    // Leaf
                    tree[childIdx] = static_cast<std::int16_t>(-(i + 1));
                } else {
                    if (tree[childIdx] == 0) {
                        int newNode = static_cast<int>(tree.size() / 2);
                        tree[childIdx] = static_cast<std::int16_t>(newNode);
                        tree.push_back(0); // left
                        tree.push_back(0); // right
                    }
                    node = tree[childIdx];
                }
            }
        }
    }

    int decode(BitReader &br) const {
        int node = 0;
        while (node >= 0) {
            int bit = br.readBits(1);
            int child = tree[(node * 2) + bit];
            if (child < 0) {
                return (-child) - 1; // Symbol
            }
            if (child == 0) {
                throw Error("deflate: invalid Huffman code in stream",
                            0, 0, "inflateRaw", "", "numkit:deflate:corrupted");
            }
            node = child;
        }
        return -1;
    }
};

// RFC 1951 Extra bits lookup tables
constexpr int kExtraLengthBits[29] = {
    0,0,0,0,0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3, 4,4,4,4, 5,5,5,5, 0
};
constexpr int kLengthBase[29] = {
    3,4,5,6,7,8,9,10, 11,13,15,17, 19,23,27,31, 35,43,51,59, 67,83,99,115, 131,163,195,227, 258
};
constexpr int kExtraDistBits[30] = {
    0,0,0,0, 1,1, 2,2, 3,3, 4,4, 5,5, 6,6, 7,7, 8,8, 9,9, 10,10, 11,11, 12,12, 13,13
};
constexpr int kDistBase[30] = {
    1,2,3,4, 5,7, 9,13, 17,25, 33,49, 65,97, 129,193, 257,385, 513,769, 1025,1537,
    2049,3073, 4097,6145, 8193,12289, 16385,24577
};
constexpr int kCodeLengthOrder[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

void buildFixedTrees(HuffmanTree &litTree, HuffmanTree &distTree) {
    std::uint8_t litLens[288];
    for (int i = 0; i <= 143; ++i) litLens[i] = 8;
    for (int i = 144; i <= 255; ++i) litLens[i] = 9;
    for (int i = 256; i <= 279; ++i) litLens[i] = 7;
    for (int i = 280; i <= 287; ++i) litLens[i] = 8;
    litTree.build(litLens, 288);

    std::uint8_t distLens[32];
    for (int i = 0; i < 32; ++i) distLens[i] = 5;
    distTree.build(distLens, 32);
}

} // anonymous namespace

// ============================================================================
// 3. Raw Deflate Inflation
// ============================================================================

std::vector<std::uint8_t> inflateRaw(const std::uint8_t *src, std::size_t len,
                                     std::size_t expectedSizeHint) {
    BitReader br(src, len);
    std::vector<std::uint8_t> out;
    if (expectedSizeHint > 0) out.reserve(expectedSizeHint);

    bool bfinal = false;
    while (!bfinal) {
        bfinal = (br.readBits(1) != 0);
        int btype = br.readBits(2);

        if (btype == 0) {
            // Uncompressed block
            br.alignToByte();
            std::uint16_t blen = br.readByte() | (static_cast<std::uint16_t>(br.readByte()) << 8);
            std::uint16_t nlen = br.readByte() | (static_cast<std::uint16_t>(br.readByte()) << 8);
            if (static_cast<std::uint16_t>(blen ^ 0xFFFFu) != nlen) {
                throw Error("deflate: uncompressed block length check failed",
                            0, 0, "inflateRaw", "", "numkit:deflate:corrupted");
            }
            for (std::uint16_t i = 0; i < blen; ++i) {
                out.push_back(br.readByte());
            }
        } else if (btype == 1 || btype == 2) {
            HuffmanTree litTree;
            HuffmanTree distTree;

            if (btype == 1) {
                // Fixed Huffman
                buildFixedTrees(litTree, distTree);
            } else {
                // Dynamic Huffman
                int hlit = br.readBits(5) + 257;
                int hdist = br.readBits(5) + 1;
                int hclen = br.readBits(4) + 4;

                std::uint8_t clCodeLens[19] = {0};
                for (int i = 0; i < hclen; ++i) {
                    clCodeLens[kCodeLengthOrder[i]] = static_cast<std::uint8_t>(br.readBits(3));
                }

                HuffmanTree clTree;
                clTree.build(clCodeLens, 19);

                std::vector<std::uint8_t> allLens(hlit + hdist, 0);
                int idx = 0;
                while (idx < hlit + hdist) {
                    int sym = clTree.decode(br);
                    if (sym < 16) {
                        allLens[idx++] = static_cast<std::uint8_t>(sym);
                    } else if (sym == 16) {
                        if (idx == 0) throw Error("deflate: invalid repeat code in dynamic header", 0, 0, "inflateRaw");
                        int rep = br.readBits(2) + 3;
                        std::uint8_t prev = allLens[idx - 1];
                        while (rep-- > 0 && idx < hlit + hdist) allLens[idx++] = prev;
                    } else if (sym == 17) {
                        int rep = br.readBits(3) + 3;
                        while (rep-- > 0 && idx < hlit + hdist) allLens[idx++] = 0;
                    } else if (sym == 18) {
                        int rep = br.readBits(7) + 11;
                        while (rep-- > 0 && idx < hlit + hdist) allLens[idx++] = 0;
                    } else {
                        throw Error("deflate: bad symbol in dynamic Huffman header", 0, 0, "inflateRaw");
                    }
                }

                litTree.build(allLens.data(), hlit);
                distTree.build(allLens.data() + hlit, hdist);
            }

            // Decode symbols
            while (true) {
                int sym = litTree.decode(br);
                if (sym < 256) {
                    out.push_back(static_cast<std::uint8_t>(sym));
                } else if (sym == 256) {
                    break; // End of block
                } else if (sym <= 285) {
                    int lenIdx = sym - 257;
                    int length = kLengthBase[lenIdx];
                    int extraBits = kExtraLengthBits[lenIdx];
                    if (extraBits > 0) length += br.readBits(extraBits);

                    int distSym = distTree.decode(br);
                    if (distSym < 0 || distSym >= 30) {
                        throw Error("deflate: invalid distance symbol", 0, 0, "inflateRaw");
                    }
                    int dist = kDistBase[distSym];
                    int extraDistBits = kExtraDistBits[distSym];
                    if (extraDistBits > 0) dist += br.readBits(extraDistBits);

                    if (static_cast<std::size_t>(dist) > out.size()) {
                        throw Error("deflate: distance beyond output buffer", 0, 0, "inflateRaw");
                    }

                    std::size_t copySrc = out.size() - dist;
                    for (int k = 0; k < length; ++k) {
                        out.push_back(out[copySrc + k]);
                    }
                } else {
                    throw Error("deflate: invalid literal/length symbol", 0, 0, "inflateRaw");
                }
            }
        } else {
            throw Error("deflate: reserved block type 3", 0, 0, "inflateRaw");
        }
    }

    return out;
}

// ============================================================================
// 4. Raw Deflate Deflation
// ============================================================================

namespace {

class BitWriter {
public:
    void writeBits(std::uint32_t val, int n) {
        bitBuf_ |= (val << bitCount_);
        bitCount_ += n;
        while (bitCount_ >= 8) {
            bytes_.push_back(static_cast<std::uint8_t>(bitBuf_ & 0xFF));
            bitBuf_ >>= 8;
            bitCount_ -= 8;
        }
    }

    void alignToByte() {
        if (bitCount_ > 0) {
            bytes_.push_back(static_cast<std::uint8_t>(bitBuf_ & 0xFF));
            bitBuf_ = 0;
            bitCount_ = 0;
        }
    }

    void writeBytes(const std::uint8_t *data, std::size_t len) {
        alignToByte();
        bytes_.insert(bytes_.end(), data, data + len);
    }

    std::vector<std::uint8_t> &data() { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
    std::uint32_t bitBuf_ = 0;
    int bitCount_ = 0;
};

// Fixed Huffman code values & lengths
constexpr std::pair<std::uint16_t, int> getFixedLitCode(int sym) {
    if (sym <= 143) return {static_cast<std::uint16_t>(0x30 + sym), 8};
    if (sym <= 255) return {static_cast<std::uint16_t>(0x190 + (sym - 144)), 9};
    if (sym <= 279) return {static_cast<std::uint16_t>(sym - 256), 7};
    return {static_cast<std::uint16_t>(0xC0 + (sym - 280)), 8};
}

// Reverse bit order for Huffman code serialization
std::uint32_t bitReverse(std::uint32_t val, int len) {
    std::uint32_t res = 0;
    for (int i = 0; i < len; ++i) {
        res = (res << 1) | ((val >> i) & 1);
    }
    return res;
}

} // anonymous namespace

std::vector<std::uint8_t> deflateRaw(const std::uint8_t *src, std::size_t len, int level) {
    BitWriter bw;

    if (level == 0) {
        // Uncompressed blocks of at most 65535 bytes
        std::size_t pos = 0;
        while (pos < len || len == 0) {
            std::size_t chunk = std::min<std::size_t>(len - pos, 65535);
            bool isLast = (pos + chunk >= len);
            bw.writeBits(isLast ? 1 : 0, 1);
            bw.writeBits(0, 2); // BTYPE = 00
            bw.alignToByte();
            std::uint16_t blen = static_cast<std::uint16_t>(chunk);
            std::uint16_t nlen = static_cast<std::uint16_t>(~blen);
            bw.writeBits(blen, 16);
            bw.writeBits(nlen, 16);
            if (chunk > 0) {
                bw.writeBytes(src + pos, chunk);
            }
            pos += chunk;
            if (pos >= len) break;
        }
        bw.alignToByte();
        return bw.data();
    }

    // Fast LZ77 with Fixed Huffman encoding
    bw.writeBits(1, 1); // BFINAL = 1
    bw.writeBits(1, 2); // BTYPE = 01 (Fixed Huffman)

    constexpr std::size_t kHashSize = 32768;
    constexpr std::size_t kWindowSize = 32768;
    constexpr std::size_t kMinMatch = 3;
    constexpr std::size_t kMaxMatch = 258;

    std::vector<std::int32_t> head(kHashSize, -1);
    std::vector<std::int32_t> prev(kWindowSize, -1);

    auto hash3 = [kHashSize](const std::uint8_t *p) -> std::size_t {
        return ((static_cast<std::size_t>(p[0]) << 10) ^
                (static_cast<std::size_t>(p[1]) << 5) ^
                static_cast<std::size_t>(p[2])) & (kHashSize - 1);
    };

    std::size_t pos = 0;
    while (pos < len) {
        std::size_t bestLen = 0;
        std::size_t bestDist = 0;

        if (pos + kMinMatch <= len) {
            std::size_t h = hash3(src + pos);
            std::int32_t matchPos = head[h];
            head[h] = static_cast<std::int32_t>(pos);

            int chainLimit = (level <= 3) ? 4 : (level <= 6 ? 16 : 64);
            while (matchPos >= 0 && (pos - matchPos) < kWindowSize && chainLimit-- > 0) {
                std::size_t curDist = pos - matchPos;
                std::size_t maxLenPossible = std::min(len - pos, kMaxMatch);
                std::size_t mlen = 0;
                while (mlen < maxLenPossible && src[pos + mlen] == src[matchPos + mlen]) {
                    ++mlen;
                }
                if (mlen > bestLen) {
                    bestLen = mlen;
                    bestDist = curDist;
                    if (bestLen >= kMaxMatch) break;
                }
                matchPos = prev[matchPos & (kWindowSize - 1)];
            }
        }

        if (bestLen >= kMinMatch) {
            // Encode length symbol
            int lenIdx = 0;
            for (int i = 0; i < 29; ++i) {
                if (kLengthBase[i] <= static_cast<int>(bestLen)) lenIdx = i;
                else break;
            }
            int litSym = 257 + lenIdx;
            auto litCode = getFixedLitCode(litSym);
            bw.writeBits(bitReverse(litCode.first, litCode.second), litCode.second);
            int extraLenBits = kExtraLengthBits[lenIdx];
            if (extraLenBits > 0) {
                bw.writeBits(static_cast<std::uint32_t>(bestLen - kLengthBase[lenIdx]), extraLenBits);
            }

            // Encode distance symbol
            int distIdx = 0;
            for (int i = 0; i < 30; ++i) {
                if (kDistBase[i] <= static_cast<int>(bestDist)) distIdx = i;
                else break;
            }
            // In Fixed Huffman, distances are 5-bit fixed codes
            bw.writeBits(bitReverse(distIdx, 5), 5);
            int extraDistBits = kExtraDistBits[distIdx];
            if (extraDistBits > 0) {
                bw.writeBits(static_cast<std::uint32_t>(bestDist - kDistBase[distIdx]), extraDistBits);
            }

            for (std::size_t k = 1; k < bestLen; ++k) {
                if (pos + k + kMinMatch <= len) {
                    std::size_t h = hash3(src + pos + k);
                    prev[(pos + k) & (kWindowSize - 1)] = head[h];
                    head[h] = static_cast<std::int32_t>(pos + k);
                }
            }
            pos += bestLen;
        } else {
            // Literal
            std::uint8_t b = src[pos++];
            auto litCode = getFixedLitCode(b);
            bw.writeBits(bitReverse(litCode.first, litCode.second), litCode.second);
        }
    }

    // End-of-block symbol (256)
    auto eob = getFixedLitCode(256);
    bw.writeBits(bitReverse(eob.first, eob.second), eob.second);
    bw.alignToByte();
    return bw.data();
}

// ============================================================================
// 5. RFC 1950 ZLIB Decompress & Compress Wrappers
// ============================================================================

std::vector<std::uint8_t> zlibDecompress(const std::uint8_t *src, std::size_t len,
                                         std::size_t expectedSizeHint) {
    if (len < 6) {
        throw Error("zlib: stream too short (< 6 bytes)",
                    0, 0, "zlibDecompress", "", "numkit:zlib:truncated");
    }

    std::uint8_t cmf = src[0];
    std::uint8_t flg = src[1];
    if (((static_cast<std::uint32_t>(cmf) << 8) + flg) % 31 != 0) {
        throw Error("zlib: invalid header checksum",
                    0, 0, "zlibDecompress", "", "numkit:zlib:badHeader");
    }
    if ((cmf & 0x0F) != 8) {
        throw Error("zlib: unsupported compression method (only Deflate supported)",
                    0, 0, "zlibDecompress", "", "numkit:zlib:badMethod");
    }
    if ((flg & 0x20) != 0) {
        throw Error("zlib: preset dictionaries not supported",
                    0, 0, "zlibDecompress", "", "numkit:zlib:fdict");
    }

    std::size_t deflateLen = len - 6;
    std::vector<std::uint8_t> uncompressed = inflateRaw(src + 2, deflateLen, expectedSizeHint);

    // Verify 4-byte big-endian Adler-32 checksum at the end
    std::uint32_t expectedAdler = (static_cast<std::uint32_t>(src[len - 4]) << 24) |
                                  (static_cast<std::uint32_t>(src[len - 3]) << 16) |
                                  (static_cast<std::uint32_t>(src[len - 2]) << 8)  |
                                  static_cast<std::uint32_t>(src[len - 1]);
    std::uint32_t actualAdler = adler32(uncompressed.data(), uncompressed.size());
    if (expectedAdler != actualAdler) {
        throw Error("zlib: Adler-32 checksum mismatch",
                    0, 0, "zlibDecompress", "", "numkit:zlib:adlerMismatch");
    }

    return uncompressed;
}

std::vector<std::uint8_t> zlibCompress(const std::uint8_t *src, std::size_t len, int level) {
    std::vector<std::uint8_t> out;
    out.reserve(len + 64);

    // Zlib header: CMF = 0x78 (Deflate, 32K window), FLG = 0x01 (level check)
    std::uint8_t cmf = 0x78;
    std::uint8_t flg = 0x01;
    // Checksum constraint: (cmf * 256 + flg) % 31 == 0
    std::uint32_t check = (static_cast<std::uint32_t>(cmf) << 8) + flg;
    std::uint32_t rem = check % 31;
    if (rem != 0) flg += static_cast<std::uint8_t>(31 - rem);

    out.push_back(cmf);
    out.push_back(flg);

    std::vector<std::uint8_t> deflated = deflateRaw(src, len, level);
    out.insert(out.end(), deflated.begin(), deflated.end());

    std::uint32_t adler = adler32(src, len);
    out.push_back(static_cast<std::uint8_t>((adler >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((adler >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((adler >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(adler & 0xFF));

    return out;
}

} // namespace numkit::ops
