// toolboxes/audio/src/io/flac_codec.cpp
//
// In-tree FLAC (Free Lossless Audio Codec) decoder.
// Zero-dependency pure C++17 implementation.

#include "flac_codec.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace numkit::audio {

bool isFlacBytes(const std::uint8_t *data, std::size_t len)
{
    if (len < 4) return false;
    return (data[0] == 'f' && data[1] == 'L' && data[2] == 'a' && data[3] == 'C');
}

bool isFlacBytes(const std::string &b)
{
    return isFlacBytes(reinterpret_cast<const std::uint8_t *>(b.data()), b.size());
}

namespace {

// Bitstream reader for FLAC MSB-first bitstreams
class BitReader {
public:
    BitReader(const uint8_t *data, size_t len)
        : data_(data), len_(len), pos_(0), bitBuf_(0), bitsLeft_(0) {}

    size_t bytePosition() const {
        return pos_ - (bitsLeft_ / 8);
    }

    bool eof() const {
        return pos_ >= len_ && bitsLeft_ == 0;
    }

    uint32_t readBits(unsigned n) {
        if (n == 0) return 0;
        while (bitsLeft_ < n) {
            if (pos_ >= len_) {
                // Return whatever bits are left shifted, or throw if empty
                if (bitsLeft_ == 0) return 0;
                uint32_t res = static_cast<uint32_t>(bitBuf_ << (n - bitsLeft_));
                bitsLeft_ = 0;
                return res;
            }
            bitBuf_ = (bitBuf_ << 8) | data_[pos_++];
            bitsLeft_ += 8;
        }
        bitsLeft_ -= n;
        uint32_t mask = (n == 32) ? 0xFFFFFFFF : ((1u << n) - 1);
        return static_cast<uint32_t>((bitBuf_ >> bitsLeft_) & mask);
    }

    uint64_t readBits64(unsigned n) {
        if (n <= 32) return readBits(n);
        uint64_t high = readBits(n - 32);
        uint64_t low = readBits(32);
        return (high << 32) | low;
    }

    int32_t readSignedBits(unsigned n) {
        uint32_t u = readBits(n);
        if (n == 0 || n >= 32) return static_cast<int32_t>(u);
        if (u & (1u << (n - 1))) {
            u |= ~((1u << n) - 1);
        }
        return static_cast<int32_t>(u);
    }

    uint32_t readUnary() {
        uint32_t count = 0;
        while (true) {
            if (bitsLeft_ == 0) {
                if (pos_ >= len_) return count;
                bitBuf_ = (bitBuf_ << 8) | data_[pos_++];
                bitsLeft_ += 8;
            }
            --bitsLeft_;
            if ((bitBuf_ >> bitsLeft_) & 1) {
                break; // Found stop bit '1'
            }
            ++count;
        }
        return count;
    }

    int32_t readRiceSigned(unsigned k) {
        uint32_t msbs = readUnary();
        uint32_t lsbs = readBits(k);
        uint32_t unsignedVal = (msbs << k) | lsbs;
        // Zig-zag / sign decoding: 0 -> 0, 1 -> -1, 2 -> 1, 3 -> -2, etc.
        if (unsignedVal & 1) {
            return -static_cast<int32_t>((unsignedVal + 1) >> 1);
        } else {
            return static_cast<int32_t>(unsignedVal >> 1);
        }
    }

    uint64_t readUtf8() {
        uint32_t v0 = readBits(8);
        if (!(v0 & 0x80)) return v0;
        if ((v0 & 0xE0) == 0xC0) {
            uint32_t v1 = readBits(8);
            return ((v0 & 0x1F) << 6) | (v1 & 0x3F);
        }
        if ((v0 & 0xF0) == 0xE0) {
            uint32_t v1 = readBits(8);
            uint32_t v2 = readBits(8);
            return ((v0 & 0x0F) << 12) | ((v1 & 0x3F) << 6) | (v2 & 0x3F);
        }
        if ((v0 & 0xF8) == 0xF0) {
            uint32_t v1 = readBits(8);
            uint32_t v2 = readBits(8);
            uint32_t v3 = readBits(8);
            return ((v0 & 0x07) << 18) | ((v1 & 0x3F) << 12) | ((v2 & 0x3F) << 6) | (v3 & 0x3F);
        }
        if ((v0 & 0xFC) == 0xF8) {
            uint64_t v = v0 & 0x03;
            for (int i = 0; i < 4; ++i) v = (v << 6) | (readBits(8) & 0x3F);
            return v;
        }
        if ((v0 & 0xFE) == 0xFC) {
            uint64_t v = v0 & 0x01;
            for (int i = 0; i < 5; ++i) v = (v << 6) | (readBits(8) & 0x3F);
            return v;
        }
        return 0;
    }

    void alignToByte() {
        bitsLeft_ &= ~7; // Discard fractional bits
    }

private:
    const uint8_t *data_;
    size_t len_;
    size_t pos_;
    uint64_t bitBuf_;
    unsigned bitsLeft_;
};

struct FlacStreamInfo {
    uint32_t minBlockSize = 0;
    uint32_t maxBlockSize = 0;
    uint32_t minFrameSize = 0;
    uint32_t maxFrameSize = 0;
    uint32_t sampleRate = 0;
    uint32_t numChannels = 0;
    uint32_t bitsPerSample = 0;
    uint64_t totalSamples = 0;
    size_t audioDataOffset = 0;
    std::string title;
    std::string artist;
    std::string comment;
};

FlacStreamInfo parseMetadata(const uint8_t *data, size_t len) {
    if (len < 42 || !isFlacBytes(data, len))
        throw Error("audioread: not a valid FLAC audio stream",
                    0, 0, "audioread", "", "numkit:audioread:badFlacMagic");

    FlacStreamInfo info;
    size_t pos = 4;
    bool isLast = false;

    while (pos + 4 <= len && !isLast) {
        isLast = (data[pos] & 0x80) != 0;
        uint8_t blockType = data[pos] & 0x7F;
        uint32_t blockSize = (static_cast<uint32_t>(data[pos + 1]) << 16) |
                             (static_cast<uint32_t>(data[pos + 2]) << 8) |
                             static_cast<uint32_t>(data[pos + 3]);
        pos += 4;
        if (pos + blockSize > len) break;

        if (blockType == 0 && blockSize >= 34) { // STREAMINFO
            BitReader br(data + pos, blockSize);
            info.minBlockSize   = br.readBits(16);
            info.maxBlockSize   = br.readBits(16);
            info.minFrameSize   = br.readBits(24);
            info.maxFrameSize   = br.readBits(24);
            info.sampleRate     = br.readBits(20);
            info.numChannels    = br.readBits(3) + 1;
            info.bitsPerSample  = br.readBits(5) + 1;
            info.totalSamples   = br.readBits64(36);
        } else if (blockType == 4 && blockSize >= 4) { // VORBIS_COMMENT
            size_t cpos = pos;
            if (cpos + 4 <= pos + blockSize) {
                uint32_t vendorLen = data[cpos] | (data[cpos+1]<<8) | (data[cpos+2]<<16) | (data[cpos+3]<<24);
                cpos += 4 + vendorLen;
                if (cpos + 4 <= pos + blockSize) {
                    uint32_t numComments = data[cpos] | (data[cpos+1]<<8) | (data[cpos+2]<<16) | (data[cpos+3]<<24);
                    cpos += 4;
                    for (uint32_t i = 0; i < numComments && cpos + 4 <= pos + blockSize; ++i) {
                        uint32_t commentLen = data[cpos] | (data[cpos+1]<<8) | (data[cpos+2]<<16) | (data[cpos+3]<<24);
                        cpos += 4;
                        if (cpos + commentLen > pos + blockSize) break;
                        std::string commentStr(reinterpret_cast<const char *>(data + cpos), commentLen);
                        cpos += commentLen;

                        size_t eq = commentStr.find('=');
                        if (eq != std::string::npos) {
                            std::string key = commentStr.substr(0, eq);
                            std::string val = commentStr.substr(eq + 1);
                            for (char &ch : key) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                            if (key == "TITLE") info.title = val;
                            else if (key == "ARTIST") info.artist = val;
                            else if (key == "COMMENT" || key == "DESCRIPTION") info.comment = val;
                        }
                    }
                }
            }
        }
        pos += blockSize;
    }

    info.audioDataOffset = pos;
    return info;
}

} // anonymous

AudioInfo peekFlac(const std::uint8_t *data, std::size_t len)
{
    FlacStreamInfo st = parseMetadata(data, len);
    AudioInfo info;
    info.format = "flac";
    info.compressionMethod = "FLAC Lossless";
    info.numChannels = st.numChannels;
    info.sampleRate = static_cast<double>(st.sampleRate);
    info.bitsPerSample = static_cast<uint16_t>(st.bitsPerSample);
    info.totalSamples = st.totalSamples;
    if (st.sampleRate > 0) {
        info.duration = static_cast<double>(st.totalSamples) / st.sampleRate;
    }
    if (info.duration > 0.0 && len > 0) {
        info.bitRate = static_cast<uint32_t>((len * 8) / (info.duration * 1000.0));
    }
    info.title = st.title;
    info.artist = st.artist;
    info.comment = st.comment;
    return info;
}

AudioData readFlac(const std::uint8_t *data, std::size_t len,
                   int64_t startSample, int64_t endSample,
                   bool nativeType,
                   std::pmr::memory_resource *mr)
{
    FlacStreamInfo streamInfo = parseMetadata(data, len);
    if (streamInfo.numChannels == 0 || streamInfo.sampleRate == 0)
        throw Error("audioread: invalid FLAC stream header",
                    0, 0, "audioread", "", "numkit:audioread:badFlacHeader");

    const size_t numChannels = streamInfo.numChannels;
    const size_t totalExpected = static_cast<size_t>(streamInfo.totalSamples);

    // Decode all frames into flat per-channel buffer
    std::vector<std::vector<int32_t>> channelBuffers(numChannels);

    BitReader br(data + streamInfo.audioDataOffset, len - streamInfo.audioDataOffset);

    // Static sample rate lookup table for FLAC frame headers
    const uint32_t sampleRateTable[16] = {
        0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000, 0, 0, 0, 0
    };

    while (!br.eof()) {
        // Sync code: 14 bits of 11111111111110 (0x3FFE)
        uint32_t sync = br.readBits(14);
        if (sync != 0x3FFE) {
            // Find next sync byte if not aligned
            while (!br.eof() && sync != 0x3FFE) {
                sync = ((sync << 1) | br.readBits(1)) & 0x3FFE;
            }
            if (sync != 0x3FFE) break;
        }

        uint32_t reserved1 = br.readBits(1);
        uint32_t blockingStrategy = br.readBits(1);
        uint32_t blockSizeCode = br.readBits(4);
        uint32_t sampleRateCode = br.readBits(4);
        uint32_t chanAssign = br.readBits(4);
        uint32_t sampleSizeCode = br.readBits(3);
        uint32_t reserved2 = br.readBits(1);
        (void)reserved1; (void)reserved2;

        if (blockSizeCode == 0) break; // Invalid

        // Frame number / sample number
        uint64_t frameOrSampleNum = br.readUtf8();
        (void)frameOrSampleNum;

        // Determine block size
        uint32_t blockSize = 0;
        if (blockSizeCode == 1) blockSize = 192;
        else if (blockSizeCode >= 2 && blockSizeCode <= 5) blockSize = 576 << (blockSizeCode - 2);
        else if (blockSizeCode == 6) blockSize = br.readBits(8) + 1;
        else if (blockSizeCode == 7) blockSize = br.readBits(16) + 1;
        else if (blockSizeCode >= 8 && blockSizeCode <= 15) blockSize = 256 << (blockSizeCode - 8);

        // Determine sample rate if needed
        if (sampleRateCode == 12) br.readBits(8);
        else if (sampleRateCode == 13 || sampleRateCode == 14) br.readBits(16);

        // CRC-8 of header
        uint32_t crc8 = br.readBits(8);
        (void)crc8;

        // Decode subframes for each channel
        std::vector<std::vector<int32_t>> subframeSamples(numChannels, std::vector<int32_t>(blockSize, 0));

        for (size_t ch = 0; ch < numChannels; ++ch) {
            uint32_t zeroBit = br.readBits(1);
            uint32_t subframeType = br.readBits(6);
            uint32_t wastedFlag = br.readBits(1);
            uint32_t wastedBits = 0;
            if (wastedFlag) wastedBits = br.readUnary() + 1;
            (void)zeroBit;

            unsigned subframeBps = streamInfo.bitsPerSample;
            if (chanAssign == 8 && ch == 1) ++subframeBps; // Side channel in Left/Side
            else if (chanAssign == 9 && ch == 0) ++subframeBps; // Side channel in Right/Side
            else if (chanAssign == 10 && ch == 1) ++subframeBps; // Side channel in Mid/Side
            subframeBps -= wastedBits;

            auto &samples = subframeSamples[ch];

            if (subframeType == 0) { // Constant
                int32_t val = br.readSignedBits(subframeBps);
                std::fill(samples.begin(), samples.end(), val);
            } else if (subframeType == 1) { // Verbatim
                for (size_t i = 0; i < blockSize; ++i) {
                    samples[i] = br.readSignedBits(subframeBps);
                }
            } else if ((subframeType & 0x38) == 0x08) { // Fixed Linear Predictor (001xxx)
                unsigned order = subframeType & 0x07;
                if (order > 4) order = 4;
                for (unsigned i = 0; i < order && i < blockSize; ++i) {
                    samples[i] = br.readSignedBits(subframeBps);
                }
                // Residual decoding
                uint32_t codingMethod = br.readBits(2);
                uint32_t partitionOrder = br.readBits(4);
                size_t numPartitions = 1u << partitionOrder;
                unsigned riceParamBits = (codingMethod == 0) ? 4 : 5;
                unsigned riceEscape = (codingMethod == 0) ? 0x0F : 0x1F;

                size_t sampleIdx = order;
                for (size_t p = 0; p < numPartitions; ++p) {
                    size_t pStart = (p == 0) ? order : (p * blockSize / numPartitions);
                    size_t pEnd = (p + 1) * blockSize / numPartitions;
                    uint32_t k = br.readBits(riceParamBits);

                    for (size_t i = pStart; i < pEnd && i < blockSize; ++i) {
                        if (k == riceEscape) {
                            unsigned escapeBits = br.readBits(5);
                            samples[i] = br.readSignedBits(escapeBits);
                        } else {
                            samples[i] = br.readRiceSigned(k);
                        }
                    }
                }

                // Restore signal from fixed predictor coefficients
                for (size_t i = order; i < blockSize; ++i) {
                    int64_t pred = 0;
                    if (order == 1) pred = samples[i - 1];
                    else if (order == 2) pred = 2 * static_cast<int64_t>(samples[i - 1]) - samples[i - 2];
                    else if (order == 3) pred = 3 * static_cast<int64_t>(samples[i - 1]) - 3 * static_cast<int64_t>(samples[i - 2]) + samples[i - 3];
                    else if (order == 4) pred = 4 * static_cast<int64_t>(samples[i - 1]) - 6 * static_cast<int64_t>(samples[i - 2]) + 4 * static_cast<int64_t>(samples[i - 3]) - samples[i - 4];
                    samples[i] += static_cast<int32_t>(pred);
                }
            } else if (subframeType >= 0x20) { // FIR LPC Predictor (1xxxxx)
                unsigned order = (subframeType & 0x1F) + 1;
                for (unsigned i = 0; i < order && i < blockSize; ++i) {
                    samples[i] = br.readSignedBits(subframeBps);
                }
                uint32_t qlpCoeffPrecision = br.readBits(4) + 1;
                int32_t qlpShift = br.readSignedBits(5);
                std::vector<int32_t> qlpCoeffs(order);
                for (unsigned i = 0; i < order; ++i) {
                    qlpCoeffs[i] = br.readSignedBits(qlpCoeffPrecision);
                }

                // Residual decoding
                uint32_t codingMethod = br.readBits(2);
                uint32_t partitionOrder = br.readBits(4);
                size_t numPartitions = 1u << partitionOrder;
                unsigned riceParamBits = (codingMethod == 0) ? 4 : 5;
                unsigned riceEscape = (codingMethod == 0) ? 0x0F : 0x1F;

                for (size_t p = 0; p < numPartitions; ++p) {
                    size_t pStart = (p == 0) ? order : (p * blockSize / numPartitions);
                    size_t pEnd = (p + 1) * blockSize / numPartitions;
                    uint32_t k = br.readBits(riceParamBits);

                    for (size_t i = pStart; i < pEnd && i < blockSize; ++i) {
                        if (k == riceEscape) {
                            unsigned escapeBits = br.readBits(5);
                            samples[i] = br.readSignedBits(escapeBits);
                        } else {
                            samples[i] = br.readRiceSigned(k);
                        }
                    }
                }

                // Restore signal from LPC predictor
                for (size_t i = order; i < blockSize; ++i) {
                    int64_t sum = 0;
                    for (unsigned j = 0; j < order; ++j) {
                        sum += static_cast<int64_t>(qlpCoeffs[j]) * samples[i - 1 - j];
                    }
                    if (qlpShift >= 0) sum >>= qlpShift;
                    else sum <<= (-qlpShift);
                    samples[i] += static_cast<int32_t>(sum);
                }
            }

            if (wastedBits > 0) {
                for (size_t i = 0; i < blockSize; ++i) {
                    samples[i] <<= wastedBits;
                }
            }
        }

        // Align and read frame footer CRC-16
        br.alignToByte();
        uint32_t crc16 = br.readBits(16);
        (void)crc16;

        // Channel decorrelation
        if (numChannels == 2) {
            auto &l = subframeSamples[0];
            auto &r = subframeSamples[1];
            if (chanAssign == 8) { // Left/Side
                for (size_t i = 0; i < blockSize; ++i) r[i] = l[i] - r[i];
            } else if (chanAssign == 9) { // Right/Side
                for (size_t i = 0; i < blockSize; ++i) l[i] = r[i] + l[i];
            } else if (chanAssign == 10) { // Mid/Side
                for (size_t i = 0; i < blockSize; ++i) {
                    int32_t mid = l[i];
                    int32_t side = r[i];
                    mid = (mid << 1) | (side & 1);
                    l[i] = (mid + side) >> 1;
                    r[i] = (mid - side) >> 1;
                }
            }
        }

        // Append to main channel buffers
        for (size_t ch = 0; ch < numChannels; ++ch) {
            channelBuffers[ch].insert(channelBuffers[ch].end(),
                                     subframeSamples[ch].begin(), subframeSamples[ch].end());
        }

        if (totalExpected > 0 && channelBuffers[0].size() >= totalExpected) {
            break;
        }
    }

    size_t availableFrames = channelBuffers.empty() ? 0 : channelBuffers[0].size();
    if (totalExpected > 0 && availableFrames > totalExpected) {
        availableFrames = totalExpected;
    }

    // Range slicing
    if (startSample < 1) startSample = 1;
    size_t s0 = static_cast<size_t>(startSample - 1);
    if (s0 >= availableFrames) s0 = availableFrames;

    size_t s1 = availableFrames;
    if (endSample > 0) {
        s1 = std::min(availableFrames, static_cast<size_t>(endSample));
    }
    if (s1 < s0) s1 = s0;

    size_t numFrames = s1 - s0;

    AudioData res;
    res.sampleRate = static_cast<double>(streamInfo.sampleRate);
    res.numChannels = static_cast<uint16_t>(numChannels);
    res.bitsPerSample = static_cast<uint16_t>(streamInfo.bitsPerSample);
    res.totalSamples = availableFrames;
    res.title = streamInfo.title;
    res.artist = streamInfo.artist;
    res.comment = streamInfo.comment;

    if (numFrames == 0) {
        res.y = Value::matrix(0, numChannels, ValueType::DOUBLE, mr);
        return res;
    }

    double scale = 1.0 / static_cast<double>(1ULL << (streamInfo.bitsPerSample - 1));

    if (!nativeType) {
        res.y = Value::matrix(numFrames, numChannels, ValueType::DOUBLE, mr);
        double *dst = res.y.doubleDataMut();
        for (size_t c = 0; c < numChannels; ++c) {
            const auto &src = channelBuffers[c];
            for (size_t t = 0; t < numFrames; ++t) {
                dst[c * numFrames + t] = static_cast<double>(src[s0 + t]) * scale;
            }
        }
    } else {
        if (streamInfo.bitsPerSample <= 16) {
            res.y = Value::matrix(numFrames, numChannels, ValueType::INT16, mr);
            int16_t *dst = res.y.int16DataMut();
            for (size_t c = 0; c < numChannels; ++c) {
                const auto &src = channelBuffers[c];
                for (size_t t = 0; t < numFrames; ++t) {
                    dst[c * numFrames + t] = static_cast<int16_t>(src[s0 + t]);
                }
            }
        } else {
            res.y = Value::matrix(numFrames, numChannels, ValueType::INT32, mr);
            int32_t *dst = res.y.int32DataMut();
            for (size_t c = 0; c < numChannels; ++c) {
                const auto &src = channelBuffers[c];
                for (size_t t = 0; t < numFrames; ++t) {
                    dst[c * numFrames + t] = src[s0 + t];
                }
            }
        }
    }

    return res;
}

} // namespace numkit::audio
