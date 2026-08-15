// toolboxes/audio/src/io/aiff_codec.cpp
//
// In-tree AIFF / AIFF-C (Apple / Big-Endian PCM) reader, writer, and peeker.
// Zero-dependency pure C++17 implementation.

#include "aiff_codec.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace numkit::audio {

bool isAiffBytes(const std::uint8_t *data, std::size_t len)
{
    if (len < 12) return false;
    if (data[0] != 'F' || data[1] != 'O' || data[2] != 'R' || data[3] != 'M') return false;
    return ((data[8] == 'A' && data[9] == 'I' && data[10] == 'F' && data[11] == 'F') ||
            (data[8] == 'A' && data[9] == 'I' && data[10] == 'F' && data[11] == 'C'));
}

bool isAiffBytes(const std::string &b)
{
    return isAiffBytes(reinterpret_cast<const std::uint8_t *>(b.data()), b.size());
}

namespace {

// Big-endian readers
inline uint16_t readU16BE(const uint8_t *p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

inline uint32_t readU32BE(const uint8_t *p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
            static_cast<uint32_t>(p[3]);
}

inline int16_t readI16BE(const uint8_t *p) {
    return static_cast<int16_t>(readU16BE(p));
}

inline int32_t readI32BE(const uint8_t *p) {
    return static_cast<int32_t>(readU32BE(p));
}

inline int32_t readI24BE(const uint8_t *p) {
    uint32_t val = (static_cast<uint32_t>(p[0]) << 16) |
                   (static_cast<uint32_t>(p[1]) << 8)  |
                    static_cast<uint32_t>(p[2]);
    if (val & 0x00800000) val |= 0xFF000000; // sign extension
    return static_cast<int32_t>(val);
}

// 80-bit IEEE 754 extended float decoder (used in AIFF sample rates)
double readExtended80BE(const uint8_t *p) {
    uint16_t exp = readU16BE(p);
    uint64_t mant = (static_cast<uint64_t>(readU32BE(p + 2)) << 32) | readU32BE(p + 6);

    if (exp == 0 && mant == 0) return 0.0;

    int sign = (exp & 0x8000) ? -1 : 1;
    exp &= 0x7FFF;

    if (exp == 0x7FFF) return 0.0; // Inf or NaN

    int unbiasedExp = static_cast<int>(exp) - 16383;
    double fraction = static_cast<double>(mant) / 9223372036854775808.0; // / 2^63

    return sign * std::ldexp(fraction, unbiasedExp);
}

// 80-bit IEEE 754 extended float encoder
void writeExtended80BE(std::vector<uint8_t> &buf, double val) {
    if (val <= 0.0) {
        for (int i = 0; i < 10; ++i) buf.push_back(0);
        return;
    }

    int exp;
    double mant = std::frexp(val, &exp);
    uint16_t biasedExp = static_cast<uint16_t>(exp + 16382);

    uint64_t mantInt = static_cast<uint64_t>(mant * 18446744073709551616.0); // * 2^64

    buf.push_back(static_cast<uint8_t>((biasedExp >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(biasedExp & 0xFF));

    for (int i = 7; i >= 0; --i) {
        buf.push_back(static_cast<uint8_t>((mantInt >> (i * 8)) & 0xFF));
    }
}

// Big-endian writers
inline void writeU16BE(std::vector<uint8_t> &buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void writeU32BE(std::vector<uint8_t> &buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void writeI24BE(std::vector<uint8_t> &buf, int32_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

struct AiffChunk {
    char id[4];
    uint32_t size;
    size_t offset;
};

std::vector<AiffChunk> findAiffChunks(const uint8_t *data, size_t len) {
    std::vector<AiffChunk> chunks;
    if (len < 12) return chunks;

    size_t pos = 12; // Skip 'FORM' + size + 'AIFF'
    while (pos + 8 <= len) {
        AiffChunk c;
        std::memcpy(c.id, data + pos, 4);
        c.size = readU32BE(data + pos + 4);
        c.offset = pos + 8;
        chunks.push_back(c);

        size_t nextPos = pos + 8 + c.size;
        if (c.size & 1) ++nextPos; // word-aligned padding
        if (nextPos <= pos || nextPos > len) break;
        pos = nextPos;
    }
    return chunks;
}

} // anonymous

AudioInfo peekAiff(const std::uint8_t *data, std::size_t len)
{
    if (!isAiffBytes(data, len))
        throw Error("audioinfo: not a valid AIFF audio stream",
                    0, 0, "audioinfo", "", "numkit:audioinfo:badAiffMagic");

    auto chunks = findAiffChunks(data, len);
    const AiffChunk *commChunk = nullptr;
    const AiffChunk *ssndChunk = nullptr;

    for (const auto &c : chunks) {
        if (std::memcmp(c.id, "COMM", 4) == 0 && !commChunk) commChunk = &c;
        else if (std::memcmp(c.id, "SSND", 4) == 0 && !ssndChunk) ssndChunk = &c;
    }

    if (!commChunk || commChunk->size < 18)
        throw Error("audioinfo: missing or malformed 'COMM' chunk in AIFF",
                    0, 0, "audioinfo", "", "numkit:audioinfo:badComm");

    const uint8_t *c = data + commChunk->offset;
    uint16_t numChannels = readU16BE(c);
    uint32_t numSampleFrames = readU32BE(c + 2);
    uint16_t sampleSize = readU16BE(c + 6);
    double sampleRate = readExtended80BE(c + 8);

    AudioInfo info;
    info.format = "aiff";
    info.compressionMethod = "Uncompressed";
    info.numChannels = numChannels;
    info.sampleRate = sampleRate;
    info.bitsPerSample = sampleSize;
    info.totalSamples = numSampleFrames;
    if (sampleRate > 0) {
        info.duration = static_cast<double>(numSampleFrames) / sampleRate;
    }
    if (info.duration > 0 && len > 0) {
        info.bitRate = static_cast<uint32_t>((len * 8) / (info.duration * 1000.0));
    }

    for (const auto &chk : chunks) {
        if (std::memcmp(chk.id, "NAME", 4) == 0 && chk.offset + chk.size <= len) {
            info.title = std::string(reinterpret_cast<const char *>(data + chk.offset), chk.size);
        } else if (std::memcmp(chk.id, "AUTH", 4) == 0 && chk.offset + chk.size <= len) {
            info.artist = std::string(reinterpret_cast<const char *>(data + chk.offset), chk.size);
        } else if ((std::memcmp(chk.id, "(c) ", 4) == 0 || std::memcmp(chk.id, "ANNO", 4) == 0) &&
                   chk.offset + chk.size <= len) {
            info.comment = std::string(reinterpret_cast<const char *>(data + chk.offset), chk.size);
        }
    }

    return info;
}

AudioData readAiff(const std::uint8_t *data, std::size_t len,
                   int64_t startSample, int64_t endSample,
                   bool nativeType,
                   std::pmr::memory_resource *mr)
{
    if (!isAiffBytes(data, len))
        throw Error("audioread: not a valid AIFF audio stream",
                    0, 0, "audioread", "", "numkit:audioread:badAiffMagic");

    auto chunks = findAiffChunks(data, len);
    const AiffChunk *commChunk = nullptr;
    const AiffChunk *ssndChunk = nullptr;

    for (const auto &c : chunks) {
        if (std::memcmp(c.id, "COMM", 4) == 0 && !commChunk) commChunk = &c;
        else if (std::memcmp(c.id, "SSND", 4) == 0 && !ssndChunk) ssndChunk = &c;
    }

    if (!commChunk || commChunk->size < 18)
        throw Error("audioread: missing or malformed 'COMM' chunk in AIFF",
                    0, 0, "audioread", "", "numkit:audioread:badComm");

    if (!ssndChunk || ssndChunk->size < 8)
        throw Error("audioread: missing 'SSND' chunk in AIFF",
                    0, 0, "audioread", "", "numkit:audioread:noSsnd");

    const uint8_t *c = data + commChunk->offset;
    uint16_t numChannels = readU16BE(c);
    uint32_t numSampleFrames = readU32BE(c + 2);
    uint16_t sampleSize = readU16BE(c + 6);
    double sampleRate = readExtended80BE(c + 8);

    if (numChannels == 0)
        throw Error("audioread: zero channels in AIFF",
                    0, 0, "audioread", "", "numkit:audioread:channels");

    uint32_t ssndOffset = readU32BE(data + ssndChunk->offset);
    const uint8_t *rawSamples = data + ssndChunk->offset + 8 + ssndOffset;

    size_t bytesPerSample = (sampleSize + 7) / 8;
    size_t blockAlign = numChannels * bytesPerSample;

    size_t availableFrames = numSampleFrames;

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
    res.numChannels = numChannels;
    res.bitsPerSample = sampleSize;
    res.totalSamples = availableFrames;

    for (const auto &chk : chunks) {
        if (std::memcmp(chk.id, "NAME", 4) == 0 && chk.offset + chk.size <= len) {
            res.title = std::string(reinterpret_cast<const char *>(data + chk.offset), chk.size);
        } else if (std::memcmp(chk.id, "AUTH", 4) == 0 && chk.offset + chk.size <= len) {
            res.artist = std::string(reinterpret_cast<const char *>(data + chk.offset), chk.size);
        } else if ((std::memcmp(chk.id, "(c) ", 4) == 0 || std::memcmp(chk.id, "ANNO", 4) == 0) &&
                   chk.offset + chk.size <= len) {
            res.comment = std::string(reinterpret_cast<const char *>(data + chk.offset), chk.size);
        }
    }

    if (numFrames == 0) {
        res.y = Value::matrix(0, numChannels, ValueType::DOUBLE, mr);
        return res;
    }

    const uint8_t *framePtr = rawSamples + s0 * blockAlign;

    if (!nativeType) {
        res.y = Value::matrix(numFrames, numChannels, ValueType::DOUBLE, mr);
        double *dst = res.y.doubleDataMut();

        for (size_t t = 0; t < numFrames; ++t) {
            const uint8_t *f = framePtr + t * blockAlign;
            for (size_t ch = 0; ch < numChannels; ++ch) {
                const uint8_t *s = f + ch * bytesPerSample;
                double val = 0.0;
                if (sampleSize == 8) {
                    val = static_cast<double>(static_cast<int8_t>(*s)) / 128.0;
                } else if (sampleSize == 16) {
                    val = static_cast<double>(readI16BE(s)) / 32768.0;
                } else if (sampleSize == 24) {
                    val = static_cast<double>(readI24BE(s)) / 8388608.0;
                } else if (sampleSize == 32) {
                    val = static_cast<double>(readI32BE(s)) / 2147483648.0;
                }
                dst[ch * numFrames + t] = val;
            }
        }
    } else {
        if (sampleSize <= 16) {
            res.y = Value::matrix(numFrames, numChannels, ValueType::INT16, mr);
            int16_t *dst = res.y.int16DataMut();
            for (size_t t = 0; t < numFrames; ++t) {
                const uint8_t *f = framePtr + t * blockAlign;
                for (size_t ch = 0; ch < numChannels; ++ch) {
                    dst[ch * numFrames + t] = (sampleSize == 8) ? (static_cast<int8_t>(f[ch]) << 8)
                                                                : readI16BE(f + ch * 2);
                }
            }
        } else {
            res.y = Value::matrix(numFrames, numChannels, ValueType::INT32, mr);
            int32_t *dst = res.y.int32DataMut();
            for (size_t t = 0; t < numFrames; ++t) {
                const uint8_t *f = framePtr + t * blockAlign;
                for (size_t ch = 0; ch < numChannels; ++ch) {
                    dst[ch * numFrames + t] = (sampleSize == 24) ? readI24BE(f + ch * 3)
                                                                 : readI32BE(f + ch * 4);
                }
            }
        }
    }

    return res;
}

std::vector<std::uint8_t> writeAiffToBytes(const Value &y, double sampleRate,
                                           uint16_t bitsPerSample,
                                           const std::string &title,
                                           const std::string &artist,
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

    size_t bytesPerSample = bitsPerSample / 8;
    size_t dataSizeBytes = numFrames * numChannels * bytesPerSample;

    std::vector<uint8_t> meta;
    if (!title.empty()) {
        meta.insert(meta.end(), {'N', 'A', 'M', 'E'});
        writeU32BE(meta, static_cast<uint32_t>(title.size()));
        meta.insert(meta.end(), title.begin(), title.end());
        if (title.size() & 1) meta.push_back(0);
    }
    if (!artist.empty()) {
        meta.insert(meta.end(), {'A', 'U', 'T', 'H'});
        writeU32BE(meta, static_cast<uint32_t>(artist.size()));
        meta.insert(meta.end(), artist.begin(), artist.end());
        if (artist.size() & 1) meta.push_back(0);
    }
    if (!comment.empty()) {
        meta.insert(meta.end(), {'A', 'N', 'N', 'O'});
        writeU32BE(meta, static_cast<uint32_t>(comment.size()));
        meta.insert(meta.end(), comment.begin(), comment.end());
        if (comment.size() & 1) meta.push_back(0);
    }

    uint32_t commSize = 18;
    uint32_t ssndSize = static_cast<uint32_t>(8 + dataSizeBytes);
    uint32_t totalFormSize = 4 + (8 + commSize) + (8 + ssndSize) + static_cast<uint32_t>(meta.size());

    std::vector<uint8_t> out;
    out.reserve(totalFormSize + 8);

    // 'FORM' Header
    out.insert(out.end(), {'F', 'O', 'R', 'M'});
    writeU32BE(out, totalFormSize);
    out.insert(out.end(), {'A', 'I', 'F', 'F'});

    // 'COMM' Chunk
    out.insert(out.end(), {'C', 'O', 'M', 'M'});
    writeU32BE(out, commSize);
    writeU16BE(out, static_cast<uint16_t>(numChannels));
    writeU32BE(out, static_cast<uint32_t>(numFrames));
    writeU16BE(out, bitsPerSample);
    writeExtended80BE(out, sampleRate);

    // 'SSND' Chunk
    out.insert(out.end(), {'S', 'S', 'N', 'D'});
    writeU32BE(out, ssndSize);
    writeU32BE(out, 0); // offset
    writeU32BE(out, 0); // blockSize

    // Interleaved Big-Endian PCM
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

    if (!meta.empty()) {
        out.insert(out.end(), meta.begin(), meta.end());
    }

    return out;
}

} // namespace numkit::audio
