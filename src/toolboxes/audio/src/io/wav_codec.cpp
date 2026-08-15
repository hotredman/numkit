// toolboxes/audio/src/io/wav_codec.cpp
//
// In-tree RIFF/WAVE reader, writer, and peeker.
// Zero-dependency pure C++17 implementation.

#include "wav_codec.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace numkit::audio {

bool isWavBytes(const std::uint8_t *data, std::size_t len)
{
    if (len < 12) return false;
    return (data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
            data[8] == 'W' && data[9] == 'A' && data[10] == 'V' && data[11] == 'E');
}

bool isWavBytes(const std::string &b)
{
    return isWavBytes(reinterpret_cast<const std::uint8_t *>(b.data()), b.size());
}

namespace {

// Little-endian readers
inline uint16_t readU16LE(const uint8_t *p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

inline uint32_t readU32LE(const uint8_t *p) {
    return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

inline int16_t readI16LE(const uint8_t *p) {
    return static_cast<int16_t>(readU16LE(p));
}

inline int32_t readI32LE(const uint8_t *p) {
    return static_cast<int32_t>(readU32LE(p));
}

inline int32_t readI24LE(const uint8_t *p) {
    uint32_t val = static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16));
    if (val & 0x00800000) val |= 0xFF000000; // sign extension
    return static_cast<int32_t>(val);
}

inline float readF32LE(const uint8_t *p) {
    float f;
    uint32_t u = readU32LE(p);
    std::memcpy(&f, &u, 4);
    return f;
}

inline double readF64LE(const uint8_t *p) {
    double d;
    uint64_t u = static_cast<uint64_t>(readU32LE(p)) |
                 (static_cast<uint64_t>(readU32LE(p + 4)) << 32);
    std::memcpy(&d, &u, 8);
    return d;
}

// Little-endian writers
inline void writeU16LE(std::vector<uint8_t> &buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

inline void writeU32LE(std::vector<uint8_t> &buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

inline void writeI24LE(std::vector<uint8_t> &buf, int32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
}

// G.711 Mu-law / A-law decoding tables
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
    int sample = 0;
    if (exponent == 0) {
        sample = (mantissa << 4) + 8;
    } else {
        sample = ((mantissa << 4) + 0x108) << (exponent - 1);
    }
    return static_cast<int16_t>(sign * sample);
}

// RIFF chunk walker
struct RiffChunk {
    char id[4];
    uint32_t size;
    size_t offset; // payload offset in data
};

std::vector<RiffChunk> findChunks(const uint8_t *data, size_t len) {
    std::vector<RiffChunk> chunks;
    if (len < 12) return chunks;

    size_t pos = 12; // Skip 'RIFF' + size + 'WAVE'
    while (pos + 8 <= len) {
        RiffChunk c;
        std::memcpy(c.id, data + pos, 4);
        c.size = readU32LE(data + pos + 4);
        c.offset = pos + 8;
        chunks.push_back(c);

        size_t nextPos = pos + 8 + c.size;
        if (c.size & 1) ++nextPos; // RIFF chunk word-alignment padding
        if (nextPos <= pos || nextPos > len) break;
        pos = nextPos;
    }
    return chunks;
}

// Parse LIST INFO metadata
void parseListInfo(const uint8_t *data, size_t len, size_t offset, size_t size,
                   std::string &title, std::string &artist, std::string &comment)
{
    if (size < 4 || offset + size > len) return;
    if (std::memcmp(data + offset, "INFO", 4) != 0) return;

    size_t pos = offset + 4;
    size_t end = offset + size;
    while (pos + 8 <= end) {
        char tag[5] = { 0 };
        std::memcpy(tag, data + pos, 4);
        uint32_t subSize = readU32LE(data + pos + 4);
        pos += 8;
        if (pos + subSize > end) break;

        std::string str(reinterpret_cast<const char *>(data + pos), subSize);
        while (!str.empty() && (str.back() == '\0' || str.back() == ' ')) str.pop_back();

        if (std::strcmp(tag, "INAM") == 0) title = str;
        else if (std::strcmp(tag, "IART") == 0) artist = str;
        else if (std::strcmp(tag, "ICMT") == 0 || std::strcmp(tag, "ICRD") == 0) comment = str;

        pos += subSize;
        if (subSize & 1) ++pos; // word alignment
    }
}

} // anonymous

AudioInfo peekWav(const std::uint8_t *data, std::size_t len)
{
    if (!isWavBytes(data, len))
        throw Error("audioinfo: not a valid RIFF/WAVE audio stream",
                    0, 0, "audioinfo", "", "numkit:audioinfo:badMagic");

    auto chunks = findChunks(data, len);
    const RiffChunk *fmtChunk = nullptr;
    const RiffChunk *dataChunk = nullptr;
    const RiffChunk *listChunk = nullptr;

    for (const auto &c : chunks) {
        if (std::memcmp(c.id, "fmt ", 4) == 0 && !fmtChunk) fmtChunk = &c;
        else if (std::memcmp(c.id, "data", 4) == 0 && !dataChunk) dataChunk = &c;
        else if (std::memcmp(c.id, "LIST", 4) == 0 && !listChunk) listChunk = &c;
    }

    if (!fmtChunk || fmtChunk->size < 16)
        throw Error("audioinfo: missing or malformed 'fmt ' chunk in WAV",
                    0, 0, "audioinfo", "", "numkit:audioinfo:badFmt");

    const uint8_t *f = data + fmtChunk->offset;
    uint16_t audioFormat = readU16LE(f);
    uint16_t numChannels = readU16LE(f + 2);
    uint32_t sampleRate  = readU32LE(f + 4);
    uint32_t byteRate    = readU32LE(f + 8);
    uint16_t blockAlign  = readU16LE(f + 12);
    uint16_t bitsPerSample = readU16LE(f + 14);

    if (audioFormat == 0xFFFE && fmtChunk->size >= 40) {
        // WAVE_FORMAT_EXTENSIBLE
        audioFormat = readU16LE(f + 24); // SubFormat GUID first 2 bytes
    }

    AudioInfo info;
    info.format = "wav";
    info.numChannels = numChannels;
    info.sampleRate = static_cast<double>(sampleRate);
    info.bitsPerSample = bitsPerSample;
    info.bitRate = (byteRate * 8) / 1000;

    switch (audioFormat) {
        case 1: info.compressionMethod = "Uncompressed"; break;
        case 3: info.compressionMethod = "IEEE Float"; break;
        case 6: info.compressionMethod = "A-law"; break;
        case 7: info.compressionMethod = "Mu-law"; break;
        default: info.compressionMethod = "Format " + std::to_string(audioFormat); break;
    }

    if (dataChunk && blockAlign > 0) {
        info.totalSamples = dataChunk->size / blockAlign;
        if (sampleRate > 0) {
            info.duration = static_cast<double>(info.totalSamples) / sampleRate;
        }
    }

    if (listChunk) {
        parseListInfo(data, len, listChunk->offset, listChunk->size,
                      info.title, info.artist, info.comment);
    }

    return info;
}

AudioData readWav(const std::uint8_t *data, std::size_t len,
                  int64_t startSample, int64_t endSample,
                  bool nativeType,
                  std::pmr::memory_resource *mr)
{
    if (!isWavBytes(data, len))
        throw Error("audioread: not a valid RIFF/WAVE audio stream",
                    0, 0, "audioread", "", "numkit:audioread:badMagic");

    auto chunks = findChunks(data, len);
    const RiffChunk *fmtChunk = nullptr;
    const RiffChunk *dataChunk = nullptr;
    const RiffChunk *listChunk = nullptr;

    for (const auto &c : chunks) {
        if (std::memcmp(c.id, "fmt ", 4) == 0 && !fmtChunk) fmtChunk = &c;
        else if (std::memcmp(c.id, "data", 4) == 0 && !dataChunk) dataChunk = &c;
        else if (std::memcmp(c.id, "LIST", 4) == 0 && !listChunk) listChunk = &c;
    }

    if (!fmtChunk || fmtChunk->size < 16)
        throw Error("audioread: missing or malformed 'fmt ' chunk in WAV",
                    0, 0, "audioread", "", "numkit:audioread:badFmt");

    if (!dataChunk)
        throw Error("audioread: missing 'data' chunk in WAV",
                    0, 0, "audioread", "", "numkit:audioread:noData");

    const uint8_t *f = data + fmtChunk->offset;
    uint16_t audioFormat = readU16LE(f);
    uint16_t numChannels = readU16LE(f + 2);
    uint32_t sampleRate  = readU32LE(f + 4);
    uint16_t blockAlign  = readU16LE(f + 12);
    uint16_t bitsPerSample = readU16LE(f + 14);

    if (audioFormat == 0xFFFE && fmtChunk->size >= 40) {
        audioFormat = readU16LE(f + 24); // SubFormat GUID
    }

    if (numChannels == 0)
        throw Error("audioread: audio has zero channels",
                    0, 0, "audioread", "", "numkit:audioread:channels");

    size_t bytesPerSample = (bitsPerSample + 7) / 8;
    size_t expectedBlockAlign = numChannels * bytesPerSample;
    if (blockAlign == 0) blockAlign = static_cast<uint16_t>(expectedBlockAlign);

    size_t availableFrames = dataChunk->size / blockAlign;
    if (availableFrames == 0) {
        AudioData res;
        res.sampleRate = sampleRate;
        res.numChannels = numChannels;
        res.bitsPerSample = bitsPerSample;
        res.totalSamples = 0;
        res.y = Value::matrix(0, numChannels, ValueType::DOUBLE, mr);
        return res;
    }

    // Determine sample range (1-based indexing)
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
    res.numChannels = numChannels;
    res.bitsPerSample = bitsPerSample;
    res.totalSamples = availableFrames;

    if (listChunk) {
        parseListInfo(data, len, listChunk->offset, listChunk->size,
                      res.title, res.artist, res.comment);
    }

    if (numFrames == 0) {
        res.y = Value::matrix(0, numChannels, ValueType::DOUBLE, mr);
        return res;
    }

    const uint8_t *rawSamples = data + dataChunk->offset + s0 * blockAlign;
    const size_t rawBytes = numFrames * blockAlign;
    if (dataChunk->offset + s0 * blockAlign + rawBytes > len)
        throw Error("audioread: truncated audio data stream",
                    0, 0, "audioread", "", "numkit:audioread:truncated");

    // Output matrix: N x C (samples down rows, channels across columns)
    if (!nativeType) {
        res.y = Value::matrix(numFrames, numChannels, ValueType::DOUBLE, mr);
        double *dst = res.y.doubleDataMut();

        for (size_t t = 0; t < numFrames; ++t) {
            const uint8_t *frame = rawSamples + t * blockAlign;
            for (size_t c = 0; c < numChannels; ++c) {
                const uint8_t *s = frame + c * bytesPerSample;
                double val = 0.0;
                if (audioFormat == 1) { // Integer PCM
                    if (bitsPerSample == 8) {
                        val = (static_cast<double>(*s) - 128.0) / 128.0;
                    } else if (bitsPerSample == 16) {
                        val = static_cast<double>(readI16LE(s)) / 32768.0;
                    } else if (bitsPerSample == 24) {
                        val = static_cast<double>(readI24LE(s)) / 8388608.0;
                    } else if (bitsPerSample == 32) {
                        val = static_cast<double>(readI32LE(s)) / 2147483648.0;
                    }
                } else if (audioFormat == 3) { // IEEE float
                    if (bitsPerSample == 32) {
                        val = static_cast<double>(readF32LE(s));
                    } else if (bitsPerSample == 64) {
                        val = readF64LE(s);
                    }
                } else if (audioFormat == 6) { // A-law
                    val = static_cast<double>(decodeALaw(*s)) / 32768.0;
                } else if (audioFormat == 7) { // Mu-law
                    val = static_cast<double>(decodeMuLaw(*s)) / 32768.0;
                }
                dst[c * numFrames + t] = val; // Column-major: column c, row t
            }
        }
    } else {
        // Native types: uint8 for 8-bit, int16 for 16-bit, int32 for 24/32-bit, single for 32-bit float
        if (audioFormat == 3 && bitsPerSample == 32) {
            res.y = Value::matrix(numFrames, numChannels, ValueType::SINGLE, mr);
            float *dst = res.y.singleDataMut();
            for (size_t t = 0; t < numFrames; ++t) {
                const uint8_t *frame = rawSamples + t * blockAlign;
                for (size_t c = 0; c < numChannels; ++c) {
                    dst[c * numFrames + t] = readF32LE(frame + c * 4);
                }
            }
        } else if (bitsPerSample == 8) {
            res.y = Value::matrix(numFrames, numChannels, ValueType::UINT8, mr);
            uint8_t *dst = res.y.uint8DataMut();
            for (size_t t = 0; t < numFrames; ++t) {
                const uint8_t *frame = rawSamples + t * blockAlign;
                for (size_t c = 0; c < numChannels; ++c) {
                    dst[c * numFrames + t] = frame[c];
                }
            }
        } else if (bitsPerSample == 16) {
            res.y = Value::matrix(numFrames, numChannels, ValueType::INT16, mr);
            int16_t *dst = res.y.int16DataMut();
            for (size_t t = 0; t < numFrames; ++t) {
                const uint8_t *frame = rawSamples + t * blockAlign;
                for (size_t c = 0; c < numChannels; ++c) {
                    dst[c * numFrames + t] = readI16LE(frame + c * 2);
                }
            }
        } else {
            res.y = Value::matrix(numFrames, numChannels, ValueType::INT32, mr);
            int32_t *dst = res.y.int32DataMut();
            for (size_t t = 0; t < numFrames; ++t) {
                const uint8_t *frame = rawSamples + t * blockAlign;
                for (size_t c = 0; c < numChannels; ++c) {
                    if (bitsPerSample == 24) dst[c * numFrames + t] = readI24LE(frame + c * 3);
                    else dst[c * numFrames + t] = readI32LE(frame + c * 4);
                }
            }
        }
    }

    return res;
}

std::vector<std::uint8_t> writeWavToBytes(const Value &y, double sampleRate,
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

    if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 &&
        bitsPerSample != 32 && bitsPerSample != 64) {
        bitsPerSample = 16;
    }

    uint16_t audioFormat = 1; // PCM
    if (y.type() == ValueType::SINGLE && bitsPerSample == 32) audioFormat = 3; // Float

    size_t bytesPerSample = bitsPerSample / 8;
    size_t blockAlign = numChannels * bytesPerSample;
    uint32_t byteRate = static_cast<uint32_t>(sampleRate * blockAlign);
    size_t dataSizeBytes = numFrames * blockAlign;

    // Optional metadata LIST INFO chunk
    std::vector<uint8_t> listInfo;
    if (!title.empty() || !artist.empty() || !comment.empty()) {
        auto appendSubChunk = [&](const char id[4], const std::string &text) {
            if (text.empty()) return;
            listInfo.insert(listInfo.end(), id, id + 4);
            uint32_t sz = static_cast<uint32_t>(text.size() + 1); // null-terminated
            writeU32LE(listInfo, sz);
            listInfo.insert(listInfo.end(), text.begin(), text.end());
            listInfo.push_back('\0');
            if (sz & 1) listInfo.push_back('\0'); // word padding
        };
        listInfo.insert(listInfo.end(), {'I', 'N', 'F', 'O'});
        appendSubChunk("INAM", title);
        appendSubChunk("IART", artist);
        appendSubChunk("ICMT", comment);
    }

    uint32_t totalRiffSize = static_cast<uint32_t>(
        4 + (8 + 16) + (8 + dataSizeBytes) + (listInfo.empty() ? 0 : 8 + listInfo.size())
    );

    std::vector<uint8_t> out;
    out.reserve(totalRiffSize + 8);

    // RIFF Header
    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    writeU32LE(out, totalRiffSize);
    out.insert(out.end(), {'W', 'A', 'V', 'E'});

    // 'fmt ' Chunk
    out.insert(out.end(), {'f', 'm', 't', ' '});
    writeU32LE(out, 16); // chunk size
    writeU16LE(out, audioFormat);
    writeU16LE(out, static_cast<uint16_t>(numChannels));
    writeU32LE(out, static_cast<uint32_t>(sampleRate));
    writeU32LE(out, byteRate);
    writeU16LE(out, static_cast<uint16_t>(blockAlign));
    writeU16LE(out, bitsPerSample);

    // 'data' Chunk
    out.insert(out.end(), {'d', 'a', 't', 'a'});
    writeU32LE(out, static_cast<uint32_t>(dataSizeBytes));

    // Encode audio samples (interleaved)
    for (size_t t = 0; t < numFrames; ++t) {
        for (size_t c = 0; c < numChannels; ++c) {
            const size_t srcIdx = c * numFrames + t;
            double sample = y.elemAsDouble(srcIdx);

            if (audioFormat == 3 && bitsPerSample == 32) {
                float f = static_cast<float>(sample);
                uint32_t u; std::memcpy(&u, &f, 4);
                writeU32LE(out, u);
            } else if (bitsPerSample == 8) {
                // 8-bit unsigned PCM [0, 255] with 128 as zero
                if (sample < -1.0) sample = -1.0;
                if (sample > 1.0) sample = 1.0;
                uint8_t u8 = static_cast<uint8_t>(std::round((sample + 1.0) * 127.5));
                out.push_back(u8);
            } else if (bitsPerSample == 16) {
                if (sample < -1.0) sample = -1.0;
                if (sample > 1.0) sample = 1.0;
                int16_t i16 = static_cast<int16_t>(std::round(sample * 32767.0));
                writeU16LE(out, static_cast<uint16_t>(i16));
            } else if (bitsPerSample == 24) {
                if (sample < -1.0) sample = -1.0;
                if (sample > 1.0) sample = 1.0;
                int32_t i24 = static_cast<int32_t>(std::round(sample * 8388607.0));
                writeI24LE(out, i24);
            } else if (bitsPerSample == 32) {
                if (sample < -1.0) sample = -1.0;
                if (sample > 1.0) sample = 1.0;
                int32_t i32 = static_cast<int32_t>(std::round(sample * 2147483647.0));
                writeU32LE(out, static_cast<uint32_t>(i32));
            }
        }
    }

    // LIST INFO Chunk
    if (!listInfo.empty()) {
        out.insert(out.end(), {'L', 'I', 'S', 'T'});
        writeU32LE(out, static_cast<uint32_t>(listInfo.size()));
        out.insert(out.end(), listInfo.begin(), listInfo.end());
    }

    return out;
}

} // namespace numkit::audio
