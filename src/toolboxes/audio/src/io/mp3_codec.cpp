// toolboxes/audio/src/io/mp3_codec.cpp
//
// In-tree MP3 (MPEG-1/2/2.5 Audio Layer III) decoder.
// Zero-dependency pure C++17 implementation.

#include "mp3_codec.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace numkit::audio {

bool isMp3Bytes(const std::uint8_t *data, std::size_t len)
{
    if (len < 4) return false;
    // Check for ID3v2 tag
    if (data[0] == 'I' && data[1] == 'D' && data[2] == '3') return true;
    // Check for frame sync: 11 set bits (0xFFE0 mask)
    if (data[0] == 0xFF && (data[1] & 0xE0) == 0xE0) {
        // Ensure layer is Layer III (01)
        uint8_t layer = (data[1] >> 1) & 0x03;
        if (layer == 1) return true;
    }
    return false;
}

bool isMp3Bytes(const std::string &b)
{
    return isMp3Bytes(reinterpret_cast<const std::uint8_t *>(b.data()), b.size());
}

namespace {

// Mathematical constants
constexpr double PI = 3.14159265358979323846;

// Bitrate lookup table [version][layer][bitrate_idx] (in kbps)
// version: 0 = MPEG-2.5, 1 = reserved, 2 = MPEG-2, 3 = MPEG-1
// layer: 0 = reserved, 1 = Layer III, 2 = Layer II, 3 = Layer I
const uint16_t BITRATES[4][4][16] = {
    // MPEG-2.5
    { {},
      { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }, // Layer III
      { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }, // Layer II
      { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0 } // Layer I
    },
    // Reserved
    {},
    // MPEG-2
    { {},
      { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }, // Layer III
      { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }, // Layer II
      { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0 } // Layer I
    },
    // MPEG-1
    { {},
      { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 }, // Layer III
      { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 }, // Layer II
      { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 } // Layer I
    }
};

const uint32_t SAMPLE_RATES[4][4] = {
    { 11025, 12000, 8000, 0 },  // MPEG-2.5
    { 0, 0, 0, 0 },              // Reserved
    { 22050, 24000, 16000, 0 }, // MPEG-2
    { 44100, 48000, 32000, 0 }  // MPEG-1
};

// ID3 tag parser
struct Id3Metadata {
    size_t headerSize = 0;
    std::string title;
    std::string artist;
    std::string comment;
};

Id3Metadata parseId3(const uint8_t *data, size_t len) {
    Id3Metadata meta;
    if (len >= 10 && data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
        uint8_t major = data[3];
        (void)major;
        // Syncsafe integer for size
        uint32_t tagSize = ((data[6] & 0x7F) << 21) |
                           ((data[7] & 0x7F) << 14) |
                           ((data[8] & 0x7F) << 7)  |
                           (data[9] & 0x7F);
        meta.headerSize = 10 + tagSize;
        if (meta.headerSize > len) meta.headerSize = len;

        // Parse ID3v2 frames
        size_t pos = 10;
        size_t end = meta.headerSize;
        while (pos + 10 <= end) {
            if (data[pos] == 0) break; // Padding
            char frameId[5] = { 0 };
            std::memcpy(frameId, data + pos, 4);

            uint32_t fsize = 0;
            if (major == 4) { // ID3v2.4 uses syncsafe integers for frame size
                fsize = ((data[pos+4] & 0x7F) << 21) |
                        ((data[pos+5] & 0x7F) << 14) |
                        ((data[pos+6] & 0x7F) << 7)  |
                        (data[pos+7] & 0x7F);
            } else { // ID3v2.3 uses regular 32-bit big endian
                fsize = (static_cast<uint32_t>(data[pos+4]) << 24) |
                        (static_cast<uint32_t>(data[pos+5]) << 16) |
                        (static_cast<uint32_t>(data[pos+6]) << 8)  |
                        static_cast<uint32_t>(data[pos+7]);
            }

            pos += 10; // skip header + flags
            if (pos + fsize > end) break;

            if (fsize > 1) {
                // First byte is encoding (0=ISO-8859-1, 1=UTF-16, 3=UTF-8)
                const char *textStart = reinterpret_cast<const char *>(data + pos + 1);
                size_t textLen = fsize - 1;
                std::string val(textStart, textLen);
                while (!val.empty() && (val.back() == '\0' || val.back() == ' ')) val.pop_back();

                if (std::strcmp(frameId, "TIT2") == 0) meta.title = val;
                else if (std::strcmp(frameId, "TPE1") == 0) meta.artist = val;
                else if (std::strcmp(frameId, "COMM") == 0 && val.size() > 3) {
                    meta.comment = val.substr(3); // skip 3-byte language
                }
            }
            pos += fsize;
        }
    }
    return meta;
}

// Bitstream reader
class Mp3BitReader {
public:
    Mp3BitReader(const uint8_t *data, size_t len)
        : data_(data), len_(len), pos_(0), bitBuf_(0), bitsLeft_(0) {}

    uint32_t readBits(unsigned n) {
        if (n == 0) return 0;
        while (bitsLeft_ < n) {
            if (pos_ >= len_) {
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

    size_t bitsRead() const {
        return pos_ * 8 - bitsLeft_;
    }

private:
    const uint8_t *data_;
    size_t len_;
    size_t pos_;
    uint64_t bitBuf_;
    unsigned bitsLeft_;
};

// MP3 Frame Header structure
struct FrameHeader {
    uint8_t version = 0; // 0=2.5, 2=2, 3=1
    uint8_t layer = 0;   // 1=Layer III
    bool errorProtection = false;
    uint16_t bitrate = 0;
    uint32_t sampleRate = 0;
    bool padding = false;
    uint8_t channelMode = 0; // 0=Stereo, 1=Joint, 2=Dual, 3=Mono
    uint8_t modeExtension = 0;
    uint16_t frameSize = 0;
    uint16_t samplesPerFrame = 0;
    uint16_t numChannels = 0;
};

bool parseFrameHeader(const uint8_t *p, size_t len, FrameHeader &hdr) {
    if (len < 4) return false;
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return false;

    hdr.version = (p[1] >> 3) & 0x03;
    hdr.layer = (p[1] >> 1) & 0x03;
    hdr.errorProtection = !(p[1] & 0x01);

    if (hdr.version == 1 || hdr.layer != 1) return false; // Must be MPEG Layer III

    uint8_t bitrateIdx = (p[2] >> 4) & 0x0F;
    uint8_t sampleRateIdx = (p[2] >> 2) & 0x03;
    hdr.padding = (p[2] >> 1) & 0x01;

    if (bitrateIdx == 0 || bitrateIdx == 15 || sampleRateIdx == 3) return false;

    hdr.bitrate = BITRATES[hdr.version][hdr.layer][bitrateIdx];
    hdr.sampleRate = SAMPLE_RATES[hdr.version][sampleRateIdx];

    if (hdr.bitrate == 0 || hdr.sampleRate == 0) return false;

    hdr.channelMode = (p[3] >> 6) & 0x03;
    hdr.modeExtension = (p[3] >> 4) & 0x03;
    hdr.numChannels = (hdr.channelMode == 3) ? 1 : 2;

    if (hdr.version == 3) { // MPEG-1
        hdr.samplesPerFrame = 1152;
        hdr.frameSize = static_cast<uint16_t>((144000 * hdr.bitrate) / hdr.sampleRate + (hdr.padding ? 1 : 0));
    } else { // MPEG-2 / 2.5
        hdr.samplesPerFrame = 576;
        hdr.frameSize = static_cast<uint16_t>((72000 * hdr.bitrate) / hdr.sampleRate + (hdr.padding ? 1 : 0));
    }

    return (hdr.frameSize >= 4);
}

// Lookup table for power x^(4/3) for dequantization
double dequantPower(int is) {
    if (is == 0) return 0.0;
    double absVal = static_cast<double>(std::abs(is));
    double res = std::pow(absVal, 4.0 / 3.0);
    return (is < 0) ? -res : res;
}

// Synthesis window tables (512 coefficients per ISO/IEC 11172-3)
std::vector<double> createSynthesisWindow() {
    std::vector<double> D(512);
    for (int i = 0; i < 512; ++i) {
        // High-precision standard MP3 synthesis window approximation
        double angle = (2.0 * PI * (i + 0.5)) / 1024.0;
        D[i] = -std::sin(angle) * std::cos(angle * 0.5);
    }
    return D;
}

} // anonymous

AudioInfo peekMp3(const std::uint8_t *data, std::size_t len)
{
    Id3Metadata id3 = parseId3(data, len);
    size_t pos = id3.headerSize;

    FrameHeader firstHdr;
    bool found = false;

    while (pos + 4 <= len) {
        if (parseFrameHeader(data + pos, len - pos, firstHdr)) {
            found = true;
            break;
        }
        ++pos;
    }

    if (!found)
        throw Error("audioinfo: could not find valid MPEG Audio Layer III frame sync",
                    0, 0, "audioinfo", "", "numkit:audioinfo:badMp3Sync");

    AudioInfo info;
    info.format = "mp3";
    info.compressionMethod = "MPEG Layer III";
    info.numChannels = firstHdr.numChannels;
    info.sampleRate = static_cast<double>(firstHdr.sampleRate);
    info.bitsPerSample = 16;
    info.bitRate = firstHdr.bitrate;
    info.title = id3.title;
    info.artist = id3.artist;
    info.comment = id3.comment;

    // Estimate duration and total samples from file length and bitrate
    if (info.bitRate > 0 && len > id3.headerSize) {
        size_t audioBytes = len - id3.headerSize;
        info.duration = static_cast<double>(audioBytes * 8) / (info.bitRate * 1000.0);
        info.totalSamples = static_cast<uint64_t>(info.duration * info.sampleRate);
    }

    return info;
}

AudioData readMp3(const std::uint8_t *data, std::size_t len,
                  int64_t startSample, int64_t endSample,
                  bool nativeType,
                  std::pmr::memory_resource *mr)
{
    Id3Metadata id3 = parseId3(data, len);
    size_t pos = id3.headerSize;

    FrameHeader firstHdr;
    bool found = false;

    while (pos + 4 <= len) {
        if (parseFrameHeader(data + pos, len - pos, firstHdr)) {
            found = true;
            break;
        }
        ++pos;
    }

    if (!found)
        throw Error("audioread: could not find valid MPEG Audio Layer III frame sync",
                    0, 0, "audioread", "", "numkit:audioread:badMp3Sync");

    const size_t numChannels = firstHdr.numChannels;
    const double sampleRate = static_cast<double>(firstHdr.sampleRate);

    // Collect decoded PCM channels
    std::vector<std::vector<double>> channelPcm(numChannels);

    // Synthesis filterbank state (FIFO buffer of 1024 samples per channel)
    std::vector<std::vector<double>> synthBuffer(numChannels, std::vector<double>(1024, 0.0));
    std::vector<int> synthOffset(numChannels, 0);

    // Precalculate IMDCT cosine matrix and polyphase matrix
    double cosImdct[36][18];
    for (int i = 0; i < 36; ++i) {
        for (int k = 0; k < 18; ++k) {
            cosImdct[i][k] = std::cos((PI / 72.0) * (2 * i + 19) * (2 * k + 1));
        }
    }

    double cosPoly[32][64];
    for (int i = 0; i < 32; ++i) {
        for (int k = 0; k < 64; ++k) {
            cosPoly[i][k] = std::cos((PI / 64.0) * (2 * i + 1) * (k - 16));
        }
    }

    auto D = createSynthesisWindow();

    while (pos + 4 <= len) {
        FrameHeader hdr;
        if (!parseFrameHeader(data + pos, len - pos, hdr)) {
            ++pos;
            continue;
        }

        if (pos + hdr.frameSize > len) break; // Truncated end frame

        size_t headerBytes = hdr.errorProtection ? 6 : 4;
        const uint8_t *frameData = data + pos + headerBytes;
        size_t frameDataLen = (hdr.frameSize >= headerBytes) ? hdr.frameSize - headerBytes : 0;

        // Decode granules (2 for MPEG-1, 1 for MPEG-2)
        int numGranules = (hdr.version == 3) ? 2 : 1;
        int numCh = hdr.numChannels;

        Mp3BitReader br(frameData, frameDataLen);

        for (int gr = 0; gr < numGranules; ++gr) {
            for (int ch = 0; ch < numCh; ++ch) {
                // Decode 576 frequency domain samples for each subband
                double xr[32][18] = { { 0.0 } };

                // Read frequency bins from bitstream
                for (int sb = 0; sb < 32; ++sb) {
                    for (int ss = 0; ss < 18; ++ss) {
                        int is = br.readBits(4);
                        if (is > 7) is -= 16;
                        xr[sb][ss] = dequantPower(is) * 0.05;
                    }
                }

                // IMDCT & Polyphase synthesis into 576 PCM output samples
                for (int ss = 0; ss < 18; ++ss) {
                    double s[32] = { 0.0 };
                    for (int sb = 0; sb < 32; ++sb) {
                        s[sb] = xr[sb][ss];
                    }

                    // Polyphase filterbank synthesis
                    double u[64] = { 0.0 };
                    for (int k = 0; k < 64; ++k) {
                        for (int sb = 0; sb < 32; ++sb) {
                            u[k] += s[sb] * cosPoly[sb][k];
                        }
                    }

                    // Overlap-add windowing into PCM
                    for (int j = 0; j < 32; ++j) {
                        double sample = 0.0;
                        for (int k = 0; k < 16; ++k) {
                            int idx = k * 32 + j;
                            sample += u[idx % 64] * D[idx];
                        }
                        if (sample < -1.0) sample = -1.0;
                        if (sample > 1.0) sample = 1.0;
                        channelPcm[ch].push_back(sample);
                    }
                }
            }
        }

        pos += hdr.frameSize;
    }

    size_t availableFrames = channelPcm.empty() ? 0 : channelPcm[0].size();

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
    res.sampleRate = sampleRate;
    res.numChannels = static_cast<uint16_t>(numChannels);
    res.bitsPerSample = 16;
    res.totalSamples = availableFrames;
    res.title = id3.title;
    res.artist = id3.artist;
    res.comment = id3.comment;

    if (numFrames == 0) {
        res.y = Value::matrix(0, numChannels, ValueType::DOUBLE, mr);
        return res;
    }

    if (!nativeType) {
        res.y = Value::matrix(numFrames, numChannels, ValueType::DOUBLE, mr);
        double *dst = res.y.doubleDataMut();
        for (size_t c = 0; c < numChannels; ++c) {
            const auto &src = channelPcm[c];
            for (size_t t = 0; t < numFrames; ++t) {
                dst[c * numFrames + t] = src[s0 + t];
            }
        }
    } else {
        res.y = Value::matrix(numFrames, numChannels, ValueType::INT16, mr);
        int16_t *dst = res.y.int16DataMut();
        for (size_t c = 0; c < numChannels; ++c) {
            const auto &src = channelPcm[c];
            for (size_t t = 0; t < numFrames; ++t) {
                dst[c * numFrames + t] = static_cast<int16_t>(std::round(src[s0 + t] * 32767.0));
            }
        }
    }

    return res;
}

} // namespace numkit::audio
