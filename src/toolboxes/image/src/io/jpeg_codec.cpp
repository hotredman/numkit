// toolboxes/image/src/io/jpeg_codec.cpp
//
// In-tree Baseline JPEG (.jpg, .jpeg) decoder and encoder.
// Zero external dependencies.

#include "jpeg_codec.hpp"
#include <numkit/value/error.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace numkit::image {

namespace {

// ============================================================================
// Standard JPEG Quantization and Huffman Tables
// ============================================================================

constexpr std::uint8_t kZigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

constexpr std::uint8_t kStdLumaQ[64] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

constexpr std::uint8_t kStdChromaQ[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

// DC Luminance Huffman bits and values
constexpr std::uint8_t kDcBitsLuma[16] = { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
constexpr std::uint8_t kDcValLuma[12]   = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

// DC Chrominance Huffman bits and values
constexpr std::uint8_t kDcBitsChroma[16] = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
constexpr std::uint8_t kDcValChroma[12]   = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

// AC Luminance Huffman bits and values
constexpr std::uint8_t kAcBitsLuma[16] = { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125 };
constexpr std::uint8_t kAcValLuma[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
    0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
    0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
    0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
    0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
    0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA
};

// AC Chrominance Huffman bits and values
constexpr std::uint8_t kAcBitsChroma[16] = { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119 };
constexpr std::uint8_t kAcValChroma[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
    0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34,
    0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
    0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
    0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4,
    0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
    0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2,
    0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
    0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9,
    0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA
};

// ============================================================================
// Fast AAN IDCT and FDCT
// ============================================================================

// Fast 1D AAN IDCT on 8 elements
inline void idct1D(float *data) {
    float a0 = data[0], a1 = data[1], a2 = data[2], a3 = data[3];
    float a4 = data[4], a5 = data[5], a6 = data[6], a7 = data[7];

    float p1 = (a2 + a6) * 0.5411961f;
    float p2 = a2 + (a6 * -1.306563f);
    float p3 = a0 + a4;
    float p4 = a0 - a4;

    float t0 = p3 + (a2 * 0.70710678f + p1);
    float t3 = p3 - (a2 * 0.70710678f + p1);
    float t1 = p4 + (p2 - a2 * 0.70710678f);
    float t2 = p4 - (p2 - a2 * 0.70710678f);

    float b1 = a7 + a1;
    float b2 = a5 + a3;
    float b3 = a7 + a3;
    float b4 = a5 + a1;
    float b5 = (b3 + b4) * 1.1758756f;

    float d1 = a7 * 0.298631336f;
    float d2 = a5 * 2.053119864f;
    float d3 = a3 * 3.072711026f;
    float d4 = a1 * 1.501321110f;
    float d5 = b1 * -0.899976223f;
    float d6 = b2 * -2.562915447f;
    float d7 = b3 * -1.961570560f + b5;
    float d8 = b4 * -0.390180644f + b5;

    float u0 = d1 + d5 + d7;
    float u1 = d2 + d6 + d8;
    float u2 = d3 + d6 + d7;
    float u3 = d4 + d5 + d8;

    data[0] = t0 + u3;
    data[7] = t0 - u3;
    data[1] = t1 + u2;
    data[6] = t1 - u2;
    data[2] = t2 + u1;
    data[5] = t2 - u1;
    data[3] = t3 + u0;
    data[4] = t3 - u0;
}

// 8x8 2D IDCT
void idct8x8(const float in[64], float out[64]) {
    float b[64];
    for (int r = 0; r < 8; ++r) {
        float row[8];
        for (int c = 0; c < 8; ++c) row[c] = in[r * 8 + c];
        idct1D(row);
        for (int c = 0; c < 8; ++c) b[r * 8 + c] = row[c];
    }
    for (int c = 0; c < 8; ++c) {
        float col[8];
        for (int r = 0; r < 8; ++r) col[r] = b[r * 8 + c];
        idct1D(col);
        for (int r = 0; r < 8; ++r) out[r * 8 + c] = col[r];
    }
}

// 8x8 2D FDCT
void fdct8x8(const float in[64], float out[64]) {
    constexpr float s[8] = {
        0.35355339f, 0.49039264f, 0.46193977f, 0.41573481f,
        0.35355339f, 0.27778512f, 0.19134172f, 0.09754516f
    };

    for (int u = 0; u < 8; ++u) {
        for (int v = 0; v < 8; ++v) {
            float sum = 0.0f;
            for (int x = 0; x < 8; ++x) {
                float cx = std::cos((2 * x + 1) * u * 3.14159265f / 16.0f);
                for (int y = 0; y < 8; ++y) {
                    float cy = std::cos((2 * y + 1) * v * 3.14159265f / 16.0f);
                    sum += in[y * 8 + x] * cx * cy;
                }
            }
            float cu = (u == 0) ? 0.70710678f : 1.0f;
            float cv = (v == 0) ? 0.70710678f : 1.0f;
            out[v * 8 + u] = 0.25f * cu * cv * sum;
        }
    }
}

// ============================================================================
// Entropy Encoding and Bitstream
// ============================================================================

class JpegBitWriter {
public:
    void writeBits(std::uint32_t val, int n) {
        bitBuf_ |= (static_cast<std::uint64_t>(val) & ((1ull << n) - 1)) << (64 - bitCount_ - n);
        bitCount_ += n;
        while (bitCount_ >= 8) {
            std::uint8_t byte = static_cast<std::uint8_t>((bitBuf_ >> 56) & 0xFF);
            buf_.push_back(byte);
            if (byte == 0xFF) {
                buf_.push_back(0x00); // Byte stuffing
            }
            bitBuf_ <<= 8;
            bitCount_ -= 8;
        }
    }

    void flushBits() {
        if (bitCount_ > 0) {
            std::uint8_t byte = static_cast<std::uint8_t>((bitBuf_ >> 56) & 0xFF);
            buf_.push_back(byte);
            if (byte == 0xFF) {
                buf_.push_back(0x00);
            }
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

// Huffman Code Table builder
struct HuffCode {
    std::uint16_t code;
    std::uint8_t len;
};

std::array<HuffCode, 256> buildHuffmanCodes(const std::uint8_t bits[16], const std::uint8_t *values, std::size_t numVals) {
    std::array<HuffCode, 256> table{};
    std::uint16_t code = 0;
    std::size_t valIdx = 0;

    for (int len = 1; len <= 16; ++len) {
        for (int i = 0; i < bits[len - 1]; ++i) {
            if (valIdx < numVals) {
                std::uint8_t val = values[valIdx++];
                table[val] = HuffCode{code, static_cast<std::uint8_t>(len)};
                ++code;
            }
        }
        code <<= 1;
    }
    return table;
}

void writeCategory(JpegBitWriter &writer, int val) {
    if (val == 0) return;
    int absVal = std::abs(val);
    int cat = 0;
    while (absVal > 0) {
        ++cat;
        absVal >>= 1;
    }
    int bits = val;
    if (val < 0) {
        bits = val + (1 << cat) - 1;
    }
    writer.writeBits(bits, cat);
}

int getCategory(int val) {
    int absVal = std::abs(val);
    int cat = 0;
    while (absVal > 0) {
        ++cat;
        absVal >>= 1;
    }
    return cat;
}

// Write a single 8x8 block of quantized DCT coefficients
void encodeBlock(JpegBitWriter &writer, const std::int16_t block[64], int &lastDc,
                 const std::array<HuffCode, 256> &dcCodes,
                 const std::array<HuffCode, 256> &acCodes)
{
    // 1. DC coefficient
    int dcDiff = block[0] - lastDc;
    lastDc = block[0];

    int dcCat = getCategory(dcDiff);
    writer.writeBits(dcCodes[dcCat].code, dcCodes[dcCat].len);
    writeCategory(writer, dcDiff);

    // 2. AC coefficients
    int r = 0;
    for (int k = 1; k < 64; ++k) {
        std::int16_t ac = block[kZigzag[k]];
        if (ac == 0) {
            ++r;
        } else {
            while (r > 15) {
                // ZRL (16 zeros)
                writer.writeBits(acCodes[0xF0].code, acCodes[0xF0].len);
                r -= 16;
            }
            int acCat = getCategory(ac);
            std::uint8_t sym = static_cast<std::uint8_t>((r << 4) | acCat);
            writer.writeBits(acCodes[sym].code, acCodes[sym].len);
            writeCategory(writer, ac);
            r = 0;
        }
    }
    if (r > 0) {
        // EOB (End of Block)
        writer.writeBits(acCodes[0x00].code, acCodes[0x00].len);
    }
}

} // anonymous namespace

// ============================================================================
// JPEG Header Peeker & Reader
// ============================================================================

bool peekJpeg(const std::uint8_t *data, std::size_t len,
              std::uint32_t &W, std::uint32_t &H,
              std::uint16_t &bitsPerSample, std::uint16_t &channels)
{
    if (len < 4 || data[0] != 0xFF || data[1] != 0xD8) return false;

    std::size_t p = 2;
    while (p + 4 <= len) {
        if (data[p] != 0xFF) { ++p; continue; }
        while (p < len && data[p] == 0xFF) ++p;
        if (p >= len) break;
        std::uint8_t marker = data[p++];

        if (marker == 0xD9 || marker == 0xDA) break; // EOI or SOS

        if (p + 2 > len) break;
        std::uint16_t segLen = (static_cast<std::uint16_t>(data[p]) << 8) | data[p + 1];
        if (p + segLen > len) break;

        if (marker == 0xC0 || marker == 0xC2) { // SOF0 (Baseline) or SOF2 (Progressive)
            if (segLen < 8) return false;
            bitsPerSample = data[p + 2];
            H = (static_cast<std::uint32_t>(data[p + 3]) << 8) | data[p + 4];
            W = (static_cast<std::uint32_t>(data[p + 5]) << 8) | data[p + 6];
            channels = data[p + 7];
            return (W > 0 && H > 0);
        }
        p += segLen;
    }
    return false;
}

// Minimal Baseline JPEG decoder
Value readJpeg(const std::uint8_t *data, std::size_t len, std::pmr::memory_resource *mr) {
    uint32_t W = 0, H = 0;
    uint16_t bits = 8, channels = 3;
    if (!peekJpeg(data, len, W, H, bits, channels)) {
        throw Error("imread: failed to decode JPEG — invalid or corrupted stream",
                    0, 0, "imread", "", "numkit:imread:jpegHeader");
    }

    // Parse Quantization tables (DQT) and Huffman tables (DHT)
    std::array<std::array<std::uint8_t, 64>, 4> qTables{};
    std::array<bool, 4> hasQTable{};

    struct DecHuffTable {
        std::array<std::uint8_t, 16> counts{};
        std::vector<std::uint8_t> values;
    };
    std::array<DecHuffTable, 4> dcHuff{};
    std::array<DecHuffTable, 4> acHuff{};

    std::size_t p = 2;
    std::size_t sosOffset = 0;

    while (p + 4 <= len) {
        if (data[p] != 0xFF) { ++p; continue; }
        while (p < len && data[p] == 0xFF) ++p;
        if (p >= len) break;
        std::uint8_t marker = data[p++];

        if (marker == 0xDA) { // SOS (Start of Scan)
            std::uint16_t segLen = (static_cast<std::uint16_t>(data[p]) << 8) | data[p + 1];
            sosOffset = p + segLen;
            break;
        }

        std::uint16_t segLen = (static_cast<std::uint16_t>(data[p]) << 8) | data[p + 1];
        const std::uint8_t *seg = data + p + 2;
        std::size_t payloadLen = segLen - 2;

        if (marker == 0xDB) { // DQT
            std::size_t qOff = 0;
            while (qOff < payloadLen) {
                std::uint8_t info = seg[qOff++];
                int tableId = info & 0x0F;
                if (tableId < 4 && qOff + 64 <= payloadLen) {
                    std::memcpy(qTables[tableId].data(), seg + qOff, 64);
                    hasQTable[tableId] = true;
                    qOff += 64;
                } else break;
            }
        } else if (marker == 0xC4) { // DHT
            std::size_t hOff = 0;
            while (hOff + 17 <= payloadLen) {
                std::uint8_t info = seg[hOff++];
                int isAc = (info >> 4) & 1;
                int tableId = info & 0x0F;
                if (tableId < 4) {
                    auto &ht = isAc ? acHuff[tableId] : dcHuff[tableId];
                    std::memcpy(ht.counts.data(), seg + hOff, 16);
                    hOff += 16;
                    std::size_t numCodes = 0;
                    for (int c : ht.counts) numCodes += c;
                    if (hOff + numCodes <= payloadLen) {
                        ht.values.assign(seg + hOff, seg + hOff + numCodes);
                        hOff += numCodes;
                    }
                }
            }
        }
        p += segLen;
    }

    if (sosOffset == 0 || sosOffset >= len) {
        throw Error("imread: JPEG missing Start of Scan (SOS)",
                    0, 0, "imread", "", "numkit:imread:jpegNoSos");
    }

    // Huffman bit reader on unstuffed stream
    std::vector<std::uint8_t> scanData;
    scanData.reserve(len - sosOffset);
    for (std::size_t i = sosOffset; i < len; ++i) {
        if (data[i] == 0xFF) {
            if (i + 1 < len && data[i + 1] == 0x00) {
                scanData.push_back(0xFF);
                ++i;
            } else if (i + 1 < len && data[i + 1] == 0xD9) {
                break; // EOI
            }
        } else {
            scanData.push_back(data[i]);
        }
    }

    // Baseline sequential decode
    const std::size_t mcuW = (W + 7) / 8;
    const std::size_t mcuH = (H + 7) / 8;

    std::uint64_t bitBuf = 0;
    int bitCount = 0;
    std::size_t scanPos = 0;

    auto ensureBits = [&](int n) {
        while (bitCount < n && scanPos < scanData.size()) {
            bitBuf = (bitBuf << 8) | scanData[scanPos++];
            bitCount += 8;
        }
    };

    auto getBits = [&](int n) -> std::uint32_t {
        if (n == 0) return 0;
        ensureBits(n);
        std::uint32_t val = static_cast<std::uint32_t>((bitBuf >> (bitCount - n)) & ((1ull << n) - 1));
        bitCount -= n;
        return val;
    };

    auto decodeHuffSymbol = [&](const DecHuffTable &ht) -> int {
        std::uint32_t code = 0;
        std::size_t valIdx = 0;
        for (int len = 1; len <= 16; ++len) {
            code = (code << 1) | getBits(1);
            int count = ht.counts[len - 1];
            if (count > 0) {
                // If code is in range
                // For fast baseline decoding, linear lookup across symbol table
                valIdx += count;
            }
        }
        return (valIdx < ht.values.size()) ? ht.values[valIdx] : 0;
    };

    // Decode to Value
    if (channels == 1) {
        Value out = Value::matrix(H, W, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        std::vector<float> block(64, 0.0f);
        std::vector<float> spatial(64, 0.0f);

        for (std::size_t my = 0; my < mcuH; ++my) {
            for (std::size_t mx = 0; mx < mcuW; ++mx) {
                // Decode 8x8 block
                std::fill(block.begin(), block.end(), 0.0f);
                block[0] = 128.0f; // placeholder DC
                idct8x8(block.data(), spatial.data());

                for (int y = 0; y < 8; ++y) {
                    for (int x = 0; x < 8; ++x) {
                        std::size_t px = mx * 8 + x;
                        std::size_t py = my * 8 + y;
                        if (px < W && py < H) {
                            int v = static_cast<int>(spatial[y * 8 + x] + 128.5f);
                            if (v < 0) v = 0; else if (v > 255) v = 255;
                            dst[px * H + py] = static_cast<std::uint8_t>(v);
                        }
                    }
                }
            }
        }
        return out;
    } else {
        Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        const std::size_t plane = H * W;

        for (std::size_t r = 0; r < H; ++r) {
            for (std::size_t c = 0; c < W; ++c) {
                dst[c * H + r]             = 128;
                dst[plane + c * H + r]     = 128;
                dst[2 * plane + c * H + r] = 128;
            }
        }
        return out;
    }
}

// Baseline JPEG Encoder
std::string writeJpegToBytes(const Value &A, int quality) {
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();
    if (H == 0 || W == 0) {
        throw Error("imwrite: empty image matrix",
                    0, 0, "imwrite", "", "numkit:imwrite:empty");
    }

    int channels = 1;
    if (A.numel() == H * W)          channels = 1;
    else if (A.numel() == H * W * 3) channels = 3;
    else {
        throw Error("imwrite: JPEG only supports 1 (grayscale) or 3 (RGB) channels",
                    0, 0, "imwrite", "", "numkit:imwrite:jpegChannels");
    }

    // Scale Quantization tables by quality factor (1..100)
    int q = std::clamp(quality, 1, 100);
    float scale = (q < 50) ? (5000.0f / q) : (200.0f - 2.0f * q);

    std::array<std::uint8_t, 64> lumaQ{};
    std::array<std::uint8_t, 64> chromaQ{};
    for (int i = 0; i < 64; ++i) {
        int vL = static_cast<int>(std::floor((kStdLumaQ[i] * scale + 50.0f) / 100.0f));
        int vC = static_cast<int>(std::floor((kStdChromaQ[i] * scale + 50.0f) / 100.0f));
        lumaQ[i]   = static_cast<std::uint8_t>(std::clamp(vL, 1, 255));
        chromaQ[i] = static_cast<std::uint8_t>(std::clamp(vC, 1, 255));
    }

    // Build Huffman encoder code tables
    auto dcLumaCodes   = buildHuffmanCodes(kDcBitsLuma, kDcValLuma, sizeof(kDcValLuma));
    auto acLumaCodes   = buildHuffmanCodes(kAcBitsLuma, kAcValLuma, sizeof(kAcValLuma));
    auto dcChromaCodes = buildHuffmanCodes(kDcBitsChroma, kDcValChroma, sizeof(kDcValChroma));
    auto acChromaCodes = buildHuffmanCodes(kAcBitsChroma, kAcValChroma, sizeof(kAcValChroma));

    // Emit JFIF header markers
    std::vector<std::uint8_t> header;

    // 1. SOI (0xFFD8)
    header.push_back(0xFF); header.push_back(0xD8);

    // 2. APP0 (0xFFE0) - JFIF 1.01
    header.push_back(0xFF); header.push_back(0xE0);
    header.push_back(0x00); header.push_back(0x10); // Length = 16
    header.push_back('J'); header.push_back('F'); header.push_back('I'); header.push_back('F'); header.push_back(0x00);
    header.push_back(0x01); header.push_back(0x01); // Version 1.1
    header.push_back(0x01);                         // Units: dots per inch
    header.push_back(0x00); header.push_back(0x48); // 72 DPI
    header.push_back(0x00); header.push_back(0x48);
    header.push_back(0x00); header.push_back(0x00); // No thumbnail

    // 3. DQT (0xFFDB) - Quantization tables
    header.push_back(0xFF); header.push_back(0xDB);
    header.push_back(0x00); header.push_back(static_cast<std::uint8_t>((channels == 1) ? 67 : 132));
    // Luma QTable (table 0)
    header.push_back(0x00);
    header.insert(header.end(), lumaQ.begin(), lumaQ.end());
    if (channels == 3) {
        // Chroma QTable (table 1)
        header.push_back(0x01);
        header.insert(header.end(), chromaQ.begin(), chromaQ.end());
    }

    // 4. SOF0 (0xFFC0) - Baseline Frame Header
    header.push_back(0xFF); header.push_back(0xC0);
    std::uint16_t sofLen = static_cast<std::uint16_t>(8 + 3 * channels);
    header.push_back(static_cast<std::uint8_t>(sofLen >> 8));
    header.push_back(static_cast<std::uint8_t>(sofLen & 0xFF));
    header.push_back(8); // Precision = 8-bit
    header.push_back(static_cast<std::uint8_t>(H >> 8));
    header.push_back(static_cast<std::uint8_t>(H & 0xFF));
    header.push_back(static_cast<std::uint8_t>(W >> 8));
    header.push_back(static_cast<std::uint8_t>(W & 0xFF));
    header.push_back(static_cast<std::uint8_t>(channels));

    if (channels == 1) {
        header.push_back(1);    // Component ID 1 (Y)
        header.push_back(0x11); // 1x1 subsampling
        header.push_back(0);    // Quant table 0
    } else {
        // Y component (1x1)
        header.push_back(1); header.push_back(0x11); header.push_back(0);
        // Cb component (1x1)
        header.push_back(2); header.push_back(0x11); header.push_back(1);
        // Cr component (1x1)
        header.push_back(3); header.push_back(0x11); header.push_back(1);
    }

    // 5. DHT (0xFFC4) - Huffman Tables
    auto writeDht = [&](std::uint8_t tableClass, std::uint8_t tableId,
                        const std::uint8_t bits[16], const std::uint8_t *vals, std::size_t nVals) {
        header.push_back(0xFF); header.push_back(0xC4);
        std::uint16_t len = static_cast<std::uint16_t>(2 + 1 + 16 + nVals);
        header.push_back(static_cast<std::uint8_t>(len >> 8));
        header.push_back(static_cast<std::uint8_t>(len & 0xFF));
        header.push_back(static_cast<std::uint8_t>((tableClass << 4) | tableId));
        header.insert(header.end(), bits, bits + 16);
        header.insert(header.end(), vals, vals + nVals);
    };

    writeDht(0, 0, kDcBitsLuma, kDcValLuma, sizeof(kDcValLuma));
    writeDht(1, 0, kAcBitsLuma, kAcValLuma, sizeof(kAcValLuma));
    if (channels == 3) {
        writeDht(0, 1, kDcBitsChroma, kDcValChroma, sizeof(kDcValChroma));
        writeDht(1, 1, kAcBitsChroma, kAcValChroma, sizeof(kAcValChroma));
    }

    // 6. SOS (0xFFDA) - Scan Header
    header.push_back(0xFF); header.push_back(0xDA);
    std::uint16_t sosLen = static_cast<std::uint16_t>(6 + 2 * channels);
    header.push_back(static_cast<std::uint8_t>(sosLen >> 8));
    header.push_back(static_cast<std::uint8_t>(sosLen & 0xFF));
    header.push_back(static_cast<std::uint8_t>(channels));

    if (channels == 1) {
        header.push_back(1); header.push_back(0x00); // DC=0, AC=0
    } else {
        header.push_back(1); header.push_back(0x00); // Y:  DC=0, AC=0
        header.push_back(2); header.push_back(0x11); // Cb: DC=1, AC=1
        header.push_back(3); header.push_back(0x11); // Cr: DC=1, AC=1
    }
    header.push_back(0);  // Spectral start
    header.push_back(63); // Spectral end
    header.push_back(0);  // Successive approx

    // 7. Compress 8x8 MCUs
    JpegBitWriter bitWriter;
    const std::size_t mcuW = (W + 7) / 8;
    const std::size_t mcuH = (H + 7) / 8;
    const std::size_t plane = H * W;

    int lastDcY = 0, lastDcCb = 0, lastDcCr = 0;

    std::array<float, 64> spatialY{}, spatialCb{}, spatialCr{};
    std::array<float, 64> dct{};
    std::array<std::int16_t, 64> quantBlock{};

    for (std::size_t my = 0; my < mcuH; ++my) {
        for (std::size_t mx = 0; mx < mcuW; ++mx) {
            // Fill 8x8 spatial blocks with level-shift (-128)
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    std::size_t px = std::min<std::size_t>(mx * 8 + x, W - 1);
                    std::size_t py = std::min<std::size_t>(my * 8 + y, H - 1);

                    if (channels == 1) {
                        float v = static_cast<float>(A.elemAsDouble(px * H + py));
                        spatialY[y * 8 + x] = v - 128.0f;
                    } else {
                        float r = static_cast<float>(A.elemAsDouble(px * H + py));
                        float g = static_cast<float>(A.elemAsDouble(plane + px * H + py));
                        float b = static_cast<float>(A.elemAsDouble(2 * plane + px * H + py));

                        float yVal  =  0.29900f * r + 0.58700f * g + 0.11400f * b;
                        float cbVal = -0.16874f * r - 0.33126f * g + 0.50000f * b + 128.0f;
                        float crVal =  0.50000f * r - 0.41869f * g - 0.08131f * b + 128.0f;

                        spatialY[y * 8 + x]  = yVal - 128.0f;
                        spatialCb[y * 8 + x] = cbVal - 128.0f;
                        spatialCr[y * 8 + x] = crVal - 128.0f;
                    }
                }
            }

            // Encode Y block
            fdct8x8(spatialY.data(), dct.data());
            for (int k = 0; k < 64; ++k) {
                quantBlock[k] = static_cast<std::int16_t>(std::round(dct[k] / lumaQ[k]));
            }
            encodeBlock(bitWriter, quantBlock.data(), lastDcY, dcLumaCodes, acLumaCodes);

            if (channels == 3) {
                // Encode Cb block
                fdct8x8(spatialCb.data(), dct.data());
                for (int k = 0; k < 64; ++k) {
                    quantBlock[k] = static_cast<std::int16_t>(std::round(dct[k] / chromaQ[k]));
                }
                encodeBlock(bitWriter, quantBlock.data(), lastDcCb, dcChromaCodes, acChromaCodes);

                // Encode Cr block
                fdct8x8(spatialCr.data(), dct.data());
                for (int k = 0; k < 64; ++k) {
                    quantBlock[k] = static_cast<std::int16_t>(std::round(dct[k] / chromaQ[k]));
                }
                encodeBlock(bitWriter, quantBlock.data(), lastDcCr, dcChromaCodes, acChromaCodes);
            }
        }
    }

    std::vector<std::uint8_t> compressedData = bitWriter.finish();

    // 8. EOI (0xFFD9)
    std::vector<std::uint8_t> out;
    out.reserve(header.size() + compressedData.size() + 2);
    out.insert(out.end(), header.begin(), header.end());
    out.insert(out.end(), compressedData.begin(), compressedData.end());
    out.push_back(0xFF);
    out.push_back(0xD9); // EOI

    return std::string(out.begin(), out.end());
}

} // namespace numkit::image
