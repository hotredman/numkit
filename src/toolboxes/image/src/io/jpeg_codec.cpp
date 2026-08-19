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

// 8x8 2D IDCT
void idct8x8(const float in[64], float out[64]) {
    static const auto T = []() {
        std::array<std::array<float, 8>, 8> mat{};
        for (int x = 0; x < 8; ++x) {
            for (int u = 0; u < 8; ++u) {
                float cu = (u == 0) ? 0.7071067811865475f : 1.0f;
                mat[x][u] = 0.5f * cu * std::cos((2 * x + 1) * u * 3.14159265358979323846f / 16.0f);
            }
        }
        return mat;
    }();

    float tmp[64];
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            float sum = 0.0f;
            for (int k = 0; k < 8; ++k) {
                sum += in[r * 8 + k] * T[c][k];
            }
            tmp[r * 8 + c] = sum;
        }
    }
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            float sum = 0.0f;
            for (int k = 0; k < 8; ++k) {
                sum += T[r][k] * tmp[k * 8 + c];
            }
            out[r * 8 + c] = sum;
        }
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

    struct ComponentInfo {
        int id = 0;
        int hSamp = 1;
        int vSamp = 1;
        int qTableId = 0;
        int dcTableId = 0;
        int acTableId = 0;
    };
    std::vector<ComponentInfo> comps;

    // Parse Quantization tables (DQT) and Huffman tables (DHT)
    std::array<std::array<std::uint8_t, 64>, 4> qTables{};
    std::array<bool, 4> hasQTable{};

    struct DecHuffTable {
        std::array<std::uint8_t, 16> counts{};
        std::vector<std::uint8_t> values;
        std::array<std::uint32_t, 17> minCode{};
        std::array<std::uint32_t, 17> maxCode{};
        std::array<int, 17> valPtr{};
        bool valid = false;

        void build() {
            std::uint32_t code = 0;
            int ptr = 0;
            for (int len = 1; len <= 16; ++len) {
                valPtr[len] = ptr;
                int cnt = counts[len - 1];
                if (cnt > 0) {
                    minCode[len] = code;
                    maxCode[len] = code + cnt - 1;
                    code += cnt;
                    ptr += cnt;
                } else {
                    minCode[len] = 0xFFFFFFFF;
                    maxCode[len] = 0;
                }
                code <<= 1;
            }
            valid = true;
        }
    };
    std::array<DecHuffTable, 4> dcHuff{};
    std::array<DecHuffTable, 4> acHuff{};

    std::size_t p = 2;
    std::size_t sosOffset = 0;
    uint16_t restartInterval = 0;

    while (p + 4 <= len) {
        if (data[p] != 0xFF) { ++p; continue; }
        while (p < len && data[p] == 0xFF) ++p;
        if (p >= len) break;
        std::uint8_t marker = data[p++];

        if (marker == 0xD9) break; // EOI

        if (p + 2 > len) break;
        std::uint16_t segLen = (static_cast<std::uint16_t>(data[p]) << 8) | data[p + 1];
        if (p + segLen > len) break;

        const std::uint8_t *seg = data + p + 2;
        std::size_t payloadLen = (segLen >= 2) ? (segLen - 2) : 0;

        if (marker == 0xC0 || marker == 0xC2) { // SOF0 / SOF2
            if (payloadLen >= 6) {
                bits = seg[0];
                H = (static_cast<std::uint32_t>(seg[1]) << 8) | seg[2];
                W = (static_cast<std::uint32_t>(seg[3]) << 8) | seg[4];
                channels = seg[5];
                comps.resize(channels);
                for (std::size_t i = 0; i < channels && 6 + i * 3 + 2 < payloadLen; ++i) {
                    comps[i].id = seg[6 + i * 3];
                    comps[i].hSamp = (seg[7 + i * 3] >> 4) & 0x0F;
                    comps[i].vSamp = seg[7 + i * 3] & 0x0F;
                    comps[i].qTableId = seg[8 + i * 3] & 0x0F;
                }
            }
        } else if (marker == 0xDA) { // SOS (Start of Scan)
            if (payloadLen >= 1) {
                std::size_t numScanComps = seg[0];
                for (std::size_t j = 0; j < numScanComps && 1 + j * 2 + 1 < payloadLen; ++j) {
                    int compId = seg[1 + j * 2];
                    int dcId = (seg[2 + j * 2] >> 4) & 0x0F;
                    int acId = seg[2 + j * 2] & 0x0F;
                    for (auto &c : comps) {
                        if (c.id == compId) {
                            c.dcTableId = dcId;
                            c.acTableId = acId;
                            break;
                        }
                    }
                }
            }
            sosOffset = p + segLen;
            break;
        } else if (marker == 0xDD) { // DRI (Define Restart Interval)
            if (payloadLen >= 2) {
                restartInterval = (static_cast<std::uint16_t>(seg[0]) << 8) | seg[1];
            }
        } else if (marker == 0xDB) { // DQT
            std::size_t qOff = 0;
            while (qOff < payloadLen) {
                std::uint8_t info = seg[qOff++];
                int tableId = info & 0x0F;
                int is16 = (info >> 4) & 1;
                if (tableId < 4) {
                    if (is16) {
                        if (qOff + 128 <= payloadLen) {
                            for (int i = 0; i < 64; ++i) {
                                qTables[tableId][i] = static_cast<std::uint8_t>(
                                    (static_cast<std::uint16_t>(seg[qOff + i * 2]) << 8) | seg[qOff + i * 2 + 1]);
                            }
                            hasQTable[tableId] = true;
                            qOff += 128;
                        } else break;
                    } else {
                        if (qOff + 64 <= payloadLen) {
                            std::memcpy(qTables[tableId].data(), seg + qOff, 64);
                            hasQTable[tableId] = true;
                            qOff += 64;
                        } else break;
                    }
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

    if (comps.empty()) {
        comps.resize(channels);
        for (std::size_t i = 0; i < channels; ++i) {
            comps[i].id = static_cast<int>(i + 1);
            comps[i].hSamp = 1;
            comps[i].vSamp = 1;
            comps[i].qTableId = (i == 0) ? 0 : 1;
            comps[i].dcTableId = (i == 0) ? 0 : 1;
            comps[i].acTableId = (i == 0) ? 0 : 1;
        }
    }

    // Initialize missing default quantization tables
    if (!hasQTable[0]) {
        std::memcpy(qTables[0].data(), kStdLumaQ, 64);
        hasQTable[0] = true;
    }
    if (!hasQTable[1]) {
        std::memcpy(qTables[1].data(), kStdChromaQ, 64);
        hasQTable[1] = true;
    }

    // Build or populate Huffman tables
    if (!dcHuff[0].counts[0] && dcHuff[0].values.empty()) {
        std::memcpy(dcHuff[0].counts.data(), kDcBitsLuma, 16);
        dcHuff[0].values.assign(kDcValLuma, kDcValLuma + sizeof(kDcValLuma));
    }
    if (!dcHuff[1].counts[0] && dcHuff[1].values.empty()) {
        std::memcpy(dcHuff[1].counts.data(), kDcBitsChroma, 16);
        dcHuff[1].values.assign(kDcValChroma, kDcValChroma + sizeof(kDcValChroma));
    }
    if (!acHuff[0].counts[0] && acHuff[0].values.empty()) {
        std::memcpy(acHuff[0].counts.data(), kAcBitsLuma, 16);
        acHuff[0].values.assign(kAcValLuma, kAcValLuma + sizeof(kAcValLuma));
    }
    if (!acHuff[1].counts[0] && acHuff[1].values.empty()) {
        std::memcpy(acHuff[1].counts.data(), kAcBitsChroma, 16);
        acHuff[1].values.assign(kAcValChroma, kAcValChroma + sizeof(kAcValChroma));
    }
    for (int i = 0; i < 4; ++i) {
        dcHuff[i].build();
        acHuff[i].build();
    }

    class JpegBitReader {
    public:
        JpegBitReader(const std::uint8_t *data, std::size_t len)
            : data_(data), len_(len), pos_(0), bitBuf_(0), bitCount_(0) {}

        void alignByte() {
            bitBuf_ = 0;
            bitCount_ = 0;
        }

        std::uint32_t getBits(int n) {
            if (n == 0) return 0;
            while (bitCount_ < n) {
                std::uint8_t b = 0;
                if (pos_ < len_) {
                    b = data_[pos_++];
                    if (b == 0xFF) {
                        while (pos_ < len_ && data_[pos_] == 0xFF) ++pos_;
                        if (pos_ < len_) {
                            std::uint8_t marker = data_[pos_++];
                            if (marker == 0x00) {
                                // byte stuffing: 0xFF 0x00 represents literal 0xFF
                            } else if (marker >= 0xD0 && marker <= 0xD7) {
                                // RST marker encountered; skip and continue reading next stream byte
                                if (pos_ < len_) b = data_[pos_++];
                                else b = 0;
                            } else if (marker == 0xD9) {
                                pos_ = len_;
                            }
                        }
                    }
                }
                bitBuf_ = (bitBuf_ << 8) | b;
                bitCount_ += 8;
            }
            std::uint32_t val = static_cast<std::uint32_t>((bitBuf_ >> (bitCount_ - n)) & ((1ull << n) - 1));
            bitCount_ -= n;
            return val;
        }

        int decodeSymbol(const DecHuffTable &ht) {
            std::uint32_t code = 0;
            for (int len = 1; len <= 16; ++len) {
                code = (code << 1) | getBits(1);
                if (code <= ht.maxCode[len] && code >= ht.minCode[len]) {
                    int idx = ht.valPtr[len] + static_cast<int>(code - ht.minCode[len]);
                    if (idx >= 0 && idx < static_cast<int>(ht.values.size())) {
                        return ht.values[idx];
                    }
                }
            }
            return 0;
        }

        int decodeValue(int cat) {
            if (cat == 0) return 0;
            std::uint32_t bits = getBits(cat);
            if (bits < (1u << (cat - 1))) {
                return static_cast<int>(bits) - ((1 << cat) - 1);
            }
            return static_cast<int>(bits);
        }

    private:
        const std::uint8_t *data_;
        std::size_t len_;
        std::size_t pos_;
        std::uint64_t bitBuf_;
        int bitCount_;
    };

    auto decodeBlock = [&](JpegBitReader &reader, const DecHuffTable &dcHt,
                           const DecHuffTable &acHt, const std::uint8_t qTable[64],
                           int &lastDc, float outSpatial[64]) {
        float freq[64] = {0};
        int dcCat = reader.decodeSymbol(dcHt);
        int dcDiff = reader.decodeValue(dcCat);
        lastDc += dcDiff;
        freq[0] = static_cast<float>(lastDc * qTable[0]);

        int k = 1;
        while (k < 64) {
            int sym = reader.decodeSymbol(acHt);
            if (sym == 0x00) break; // EOB
            if (sym == 0xF0) { // ZRL
                k += 16;
                continue;
            }
            int run = (sym >> 4) & 0x0F;
            int cat = sym & 0x0F;
            k += run;
            if (k >= 64) break;
            int val = reader.decodeValue(cat);
            int zz = kZigzag[k];
            freq[zz] = static_cast<float>(val * qTable[k]);
            ++k;
        }
        idct8x8(freq, outSpatial);
    };

    int maxH = 1, maxV = 1;
    for (const auto &c : comps) {
        maxH = std::max(maxH, c.hSamp);
        maxV = std::max(maxV, c.vSamp);
    }

    const std::size_t mcuW = (W + maxH * 8 - 1) / (maxH * 8);
    const std::size_t mcuH = (H + maxV * 8 - 1) / (maxV * 8);

    std::vector<std::vector<float>> compPlanes(comps.size());
    std::vector<std::size_t> compW(comps.size());
    std::vector<std::size_t> compH(comps.size());

    for (std::size_t i = 0; i < comps.size(); ++i) {
        compW[i] = mcuW * comps[i].hSamp * 8;
        compH[i] = mcuH * comps[i].vSamp * 8;
        compPlanes[i].resize(compW[i] * compH[i], 0.0f);
    }

    std::vector<int> lastDc(comps.size(), 0);
    JpegBitReader reader(data + sosOffset, len - sosOffset);
    std::size_t mcuCount = 0;

    for (std::size_t my = 0; my < mcuH; ++my) {
        for (std::size_t mx = 0; mx < mcuW; ++mx) {
            if (restartInterval > 0 && mcuCount > 0 && (mcuCount % restartInterval == 0)) {
                reader.alignByte();
                for (auto &dc : lastDc) dc = 0;
            }
            for (std::size_t ci = 0; ci < comps.size(); ++ci) {
                const auto &c = comps[ci];
                for (int v = 0; v < c.vSamp; ++v) {
                    for (int h = 0; h < c.hSamp; ++h) {
                        float spatial[64];
                        decodeBlock(reader, dcHuff[c.dcTableId], acHuff[c.acTableId],
                                    qTables[c.qTableId].data(), lastDc[ci], spatial);
                        std::size_t blockRow = (my * c.vSamp + v) * 8;
                        std::size_t blockCol = (mx * c.hSamp + h) * 8;
                        for (int by = 0; by < 8; ++by) {
                            for (int bx = 0; bx < 8; ++bx) {
                                compPlanes[ci][(blockRow + by) * compW[ci] + (blockCol + bx)] = spatial[by * 8 + bx];
                            }
                        }
                    }
                }
            }
            ++mcuCount;
        }
    }

    if (comps.size() == 1) {
        Value out = Value::matrix(H, W, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        for (std::size_t r = 0; r < H; ++r) {
            for (std::size_t c = 0; c < W; ++c) {
                float y = compPlanes[0][r * compW[0] + c] + 128.5f;
                int val = static_cast<int>(y);
                if (val < 0) val = 0; else if (val > 255) val = 255;
                dst[c * H + r] = static_cast<std::uint8_t>(val);
            }
        }
        return out;
    } else {
        Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
        std::uint8_t *dst = out.uint8DataMut();
        const std::size_t plane = H * W;
        for (std::size_t r = 0; r < H; ++r) {
            for (std::size_t c = 0; c < W; ++c) {
                float y = compPlanes[0][r * compW[0] + c];

                std::size_t cbR = (comps[1].vSamp == maxV) ? r : (r * comps[1].vSamp / maxV);
                std::size_t cbC = (comps[1].hSamp == maxH) ? c : (c * comps[1].hSamp / maxH);
                std::size_t crR = (comps[2].vSamp == maxV) ? r : (r * comps[2].vSamp / maxV);
                std::size_t crC = (comps[2].hSamp == maxH) ? c : (c * comps[2].hSamp / maxH);

                float cb = compPlanes[1][cbR * compW[1] + cbC];
                float cr = compPlanes[2][crR * compW[2] + crC];

                int R = static_cast<int>(y + 1.402f * cr + 128.5f);
                int G = static_cast<int>(y - 0.344136f * cb - 0.714136f * cr + 128.5f);
                int B = static_cast<int>(y + 1.772f * cb + 128.5f);

                if (R < 0) R = 0; else if (R > 255) R = 255;
                if (G < 0) G = 0; else if (G > 255) G = 255;
                if (B < 0) B = 0; else if (B > 255) B = 255;

                dst[c * H + r]             = static_cast<std::uint8_t>(R);
                dst[plane + c * H + r]     = static_cast<std::uint8_t>(G);
                dst[2 * plane + c * H + r] = static_cast<std::uint8_t>(B);
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
