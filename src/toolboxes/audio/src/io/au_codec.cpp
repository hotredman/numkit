// toolboxes/audio/src/io/au_codec.cpp
//
// In-tree Sun/NeXT AU (.au, .snd) reader, writer, and peeker.
// Zero-dependency pure C++17 implementation.

#include "au_codec.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace numkit::audio {

bool isAuBytes(const std::uint8_t *data, std::size_t len)
{
    if (len < 4) return false;
    return (data[0] == '.' && data[1] == 's' && data[2] == 'n' && data[3] == 'd');
}

bool isAuBytes(const std::string &b)
{
    return isAuBytes(reinterpret_cast<const std::uint8_t *>(b.data()), b.size());
}

namespace {

inline uint32_t readU32BE(const uint8_t *p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
            static_cast<uint32_t>(p[3]);
}

inline int16_t readI16BE(const uint8_t *p) {
    return static_cast<int16_t>((p[0] << 8) | p[1]);
}

inline int32_t readI32BE(const uint8_t *p) {
    return static_cast<int32_t>(readU32BE(p));
}

inline int32_t readI24BE(const uint8_t *p) {
    uint32_t val = (static_cast<uint32_t>(p[0]) << 16) |
                   (static_cast<uint32_t>(p[1]) << 8)  |
                    static_cast<uint32_t>(p[2]);
    if (val & 0x00800000) val |= 0xFF000000;
    return static_cast<int32_t>(val);
}

inline float readF32BE(const uint8_t *p) {
    float f;
    uint32_t u = readU32BE(p);
    std::memcpy(&f, &u, 4);
    return f;
}

inline double readF64BE(const uint8_t *p) {
    double d;
    uint64_t u = (static_cast<uint64_t>(readU32BE(p)) << 32) | readU32BE(p + 4);
    std::memcpy(&d, &u, 8);
    return d;
}

// Mu-law and A-law decoding tables
int16_t decodeMuLaw(uint8_t mu) {
    mu = ~mu;
    int sign = (mu & 0x80) ? -1 : 1;
    int exponent = (mu >> 4) & 0x07;
    int mantissa = mu & 0x0F;
    int sample = ((mantissa << 3) + 0x84) << exponent;
    return static_cast<int16_t>(sign * (sample - 0x84));
}

int16_t decodeALaw(uint8_t a) {
    a ^= 0x55;
    int sign = (a & 0x80) ? -1 : 1;
    int exponent = (a >> 4) & 0x07;
    int mantissa = a & 0x0F;
    int sample = (exponent == 0) ? ((mantissa << 4) + 8) : (((mantissa << 4) + 0x108) << (exponent - 1));
    return static_cast<int16_t>(sign * sample);
}

inline void writeU32BE(std::vector<uint8_t> &buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void writeU16BE(std::vector<uint8_t> &buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void writeI24BE(std::vector<uint8_t> &buf, int32_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

} // anonymous

AudioInfo peekAu(const std::uint8_t *data, std::size_t len)
{
    if (!isAuBytes(data, len) || len < 24)
        throw Error("audioinfo: not a valid AU/SND audio stream",
                    0, 0, "audioinfo", "", "numkit:audioinfo:badAuMagic");

    uint32_t dataOffset = readU32BE(data + 4);
    uint32_t dataSize   = readU32BE(data + 8);
    uint32_t encoding   = readU32BE(data + 12);
    uint32_t sampleRate = readU32BE(data + 16);
    uint32_t numChannels = readU32BE(data + 20);

    AudioInfo info;
    info.format = "au";
    info.numChannels = numChannels;
    info.sampleRate = static_cast<double>(sampleRate);

    size_t bytesPerSample = 2;
    switch (encoding) {
        case 1: info.compressionMethod = "Mu-law"; info.bitsPerSample = 8; bytesPerSample = 1; break;
        case 2: info.compressionMethod = "Linear 8-bit PCM"; info.bitsPerSample = 8; bytesPerSample = 1; break;
        case 3: info.compressionMethod = "Linear 16-bit PCM"; info.bitsPerSample = 16; bytesPerSample = 2; break;
        case 4: info.compressionMethod = "Linear 24-bit PCM"; info.bitsPerSample = 24; bytesPerSample = 3; break;
        case 5: info.compressionMethod = "Linear 32-bit PCM"; info.bitsPerSample = 32; bytesPerSample = 4; break;
        case 6: info.compressionMethod = "IEEE 32-bit Float"; info.bitsPerSample = 32; bytesPerSample = 4; break;
        case 7: info.compressionMethod = "IEEE 64-bit Float"; info.bitsPerSample = 64; bytesPerSample = 8; break;
        case 27: info.compressionMethod = "A-law"; info.bitsPerSample = 8; bytesPerSample = 1; break;
        default: info.compressionMethod = "Encoding " + std::to_string(encoding); break;
    }

    if (dataOffset > 24 && dataOffset <= len) {
        info.comment = std::string(reinterpret_cast<const char *>(data + 24), dataOffset - 24);
        while (!info.comment.empty() && (info.comment.back() == '\0' || info.comment.back() == ' '))
            info.comment.pop_back();
    }

    size_t actualDataSize = (dataSize != 0xFFFFFFFF && dataSize > 0) ? dataSize : (len > dataOffset ? len - dataOffset : 0);
    size_t blockAlign = numChannels * bytesPerSample;
    if (blockAlign > 0) {
        info.totalSamples = actualDataSize / blockAlign;
        if (sampleRate > 0) {
            info.duration = static_cast<double>(info.totalSamples) / sampleRate;
        }
    }

    if (info.duration > 0.0 && len > 0) {
        info.bitRate = static_cast<uint32_t>((len * 8) / (info.duration * 1000.0));
    }

    return info;
}

AudioData readAu(const std::uint8_t *data, std::size_t len,
                 int64_t startSample, int64_t endSample,
                 bool nativeType,
                 std::pmr::memory_resource *mr)
{
    if (!isAuBytes(data, len) || len < 24)
        throw Error("audioread: not a valid AU/SND audio stream",
                    0, 0, "audioread", "", "numkit:audioread:badAuMagic");

    uint32_t dataOffset = readU32BE(data + 4);
    uint32_t dataSize   = readU32BE(data + 8);
    uint32_t encoding   = readU32BE(data + 12);
    uint32_t sampleRate = readU32BE(data + 16);
    uint32_t numChannels = readU32BE(data + 20);

    if (numChannels == 0)
        throw Error("audioread: audio has zero channels",
                    0, 0, "audioread", "", "numkit:audioread:channels");

    size_t bytesPerSample = 2;
    uint16_t bitsPerSample = 16;
    if (encoding == 1 || encoding == 2 || encoding == 27) { bytesPerSample = 1; bitsPerSample = 8; }
    else if (encoding == 3) { bytesPerSample = 2; bitsPerSample = 16; }
    else if (encoding == 4) { bytesPerSample = 3; bitsPerSample = 24; }
    else if (encoding == 5 || encoding == 6) { bytesPerSample = 4; bitsPerSample = 32; }
    else if (encoding == 7) { bytesPerSample = 8; bitsPerSample = 64; }

    size_t blockAlign = numChannels * bytesPerSample;
    size_t actualDataBytes = (dataSize != 0xFFFFFFFF && dataSize > 0) ? dataSize : (len > dataOffset ? len - dataOffset : 0);
    size_t availableFrames = actualDataBytes / blockAlign;

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
    res.sampleRate = static_cast<double>(sampleRate);
    res.numChannels = static_cast<uint16_t>(numChannels);
    res.bitsPerSample = bitsPerSample;
    res.totalSamples = availableFrames;

    if (dataOffset > 24 && dataOffset <= len) {
        res.comment = std::string(reinterpret_cast<const char *>(data + 24), dataOffset - 24);
        while (!res.comment.empty() && (res.comment.back() == '\0' || res.comment.back() == ' '))
            res.comment.pop_back();
    }

    if (numFrames == 0) {
        res.y = Value::matrix(0, numChannels, ValueType::DOUBLE, mr);
        return res;
    }

    const uint8_t *rawSamples = data + dataOffset + s0 * blockAlign;

    if (!nativeType) {
        res.y = Value::matrix(numFrames, numChannels, ValueType::DOUBLE, mr);
        double *dst = res.y.doubleDataMut();

        for (size_t t = 0; t < numFrames; ++t) {
            const uint8_t *frame = rawSamples + t * blockAlign;
            for (size_t ch = 0; ch < numChannels; ++ch) {
                const uint8_t *s = frame + ch * bytesPerSample;
                double val = 0.0;
                if (encoding == 1) { // Mu-law
                    val = static_cast<double>(decodeMuLaw(*s)) / 32768.0;
                } else if (encoding == 2) { // 8-bit linear signed
                    val = static_cast<double>(static_cast<int8_t>(*s)) / 128.0;
                } else if (encoding == 3) { // 16-bit linear signed BE
                    val = static_cast<double>(readI16BE(s)) / 32768.0;
                } else if (encoding == 4) { // 24-bit linear signed BE
                    val = static_cast<double>(readI24BE(s)) / 8388608.0;
                } else if (encoding == 5) { // 32-bit linear signed BE
                    val = static_cast<double>(readI32BE(s)) / 2147483648.0;
                } else if (encoding == 6) { // 32-bit IEEE float BE
                    val = static_cast<double>(readF32BE(s));
                } else if (encoding == 7) { // 64-bit IEEE float BE
                    val = readF64BE(s);
                } else if (encoding == 27) { // A-law
                    val = static_cast<double>(decodeALaw(*s)) / 32768.0;
                }
                dst[ch * numFrames + t] = val;
            }
        }
    } else {
        if (bitsPerSample <= 16) {
            res.y = Value::matrix(numFrames, numChannels, ValueType::INT16, mr);
            int16_t *dst = res.y.int16DataMut();
            for (size_t t = 0; t < numFrames; ++t) {
                const uint8_t *frame = rawSamples + t * blockAlign;
                for (size_t ch = 0; ch < numChannels; ++ch) {
                    dst[ch * numFrames + t] = (bitsPerSample == 8) ? (static_cast<int8_t>(frame[ch]) << 8)
                                                                   : readI16BE(frame + ch * 2);
                }
            }
        } else {
            res.y = Value::matrix(numFrames, numChannels, ValueType::INT32, mr);
            int32_t *dst = res.y.int32DataMut();
            for (size_t t = 0; t < numFrames; ++t) {
                const uint8_t *frame = rawSamples + t * blockAlign;
                for (size_t ch = 0; ch < numChannels; ++ch) {
                    dst[ch * numFrames + t] = (bitsPerSample == 24) ? readI24BE(frame + ch * 3)
                                                                    : readI32BE(frame + ch * 4);
                }
            }
        }
    }

    return res;
}

std::vector<std::uint8_t> writeAuToBytes(const Value &y, double sampleRate,
                                         uint16_t bitsPerSample,
                                         const std::string &comment)
{
    const auto &d = y.dims();
    const size_t numFrames = d.rows();
    const size_t numChannels = (d.ndim() >= 2) ? d.cols() : 1;

    if (numFrames == 0 || numChannels == 0)
        throw Error("audiowrite: audio matrix cannot be empty",
                    0, 0, "audiowrite", "", "numkit:audiowrite:empty");

    if (sampleRate <= 0) sampleRate = 44100.0;
    if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) {
        bitsPerSample = 16;
    }

    uint32_t encoding = 3; // 16-bit PCM by default
    size_t bytesPerSample = 2;
    if (bitsPerSample == 8) { encoding = 2; bytesPerSample = 1; }
    else if (bitsPerSample == 16) { encoding = 3; bytesPerSample = 2; }
    else if (bitsPerSample == 24) { encoding = 4; bytesPerSample = 3; }
    else if (bitsPerSample == 32) { encoding = 5; bytesPerSample = 4; }

    size_t commentSize = comment.size();
    size_t headerSize = 24 + commentSize;
    if (headerSize & 7) headerSize += 8 - (headerSize & 7); // 8-byte aligned header

    size_t dataSizeBytes = numFrames * numChannels * bytesPerSample;

    std::vector<uint8_t> out;
    out.reserve(headerSize + dataSizeBytes);

    // '.snd' Magic
    out.insert(out.end(), {'.', 's', 'n', 'd'});
    writeU32BE(out, static_cast<uint32_t>(headerSize));
    writeU32BE(out, static_cast<uint32_t>(dataSizeBytes));
    writeU32BE(out, encoding);
    writeU32BE(out, static_cast<uint32_t>(sampleRate));
    writeU32BE(out, static_cast<uint32_t>(numChannels));

    // Comment string
    if (!comment.empty()) {
        out.insert(out.end(), comment.begin(), comment.end());
    }
    while (out.size() < headerSize) out.push_back(0); // padding

    // Big-Endian PCM data
    for (size_t t = 0; t < numFrames; ++t) {
        for (size_t ch = 0; ch < numChannels; ++ch) {
            double sample = y.elemAsDouble(ch * numFrames + t);
            if (sample < -1.0) sample = -1.0;
            if (sample > 1.0) sample = 1.0;

            if (bitsPerSample == 8) {
                int8_t s8 = static_cast<int8_t>(std::round(sample * 127.0));
                out.push_back(static_cast<uint8_t>(s8));
            } else if (bitsPerSample == 16) {
                int16_t s16 = static_cast<int16_t>(std::round(sample * 32767.0));
                writeU16BE(out, static_cast<uint16_t>(s16));
            } else if (bitsPerSample == 24) {
                int32_t s24 = static_cast<int32_t>(std::round(sample * 8388607.0));
                writeI24BE(out, s24);
            } else if (bitsPerSample == 32) {
                int32_t s32 = static_cast<int32_t>(std::round(sample * 2147483647.0));
                writeU32BE(out, static_cast<uint32_t>(s32));
            }
        }
    }

    return out;
}

} // namespace numkit::audio
