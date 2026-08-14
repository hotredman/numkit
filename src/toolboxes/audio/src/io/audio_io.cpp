// toolboxes/audio/src/io/audio_io.cpp
//
// Unified Audio I/O dispatcher for NumKit: audioread, audiowrite, audioinfo.
// Zero-dependency pure C++17 implementation.

#include <numkit/audio/io/audio_io.hpp>

#include "wav_codec.hpp"
#include "flac_codec.hpp"
#include "mp3_codec.hpp"
#include "aiff_codec.hpp"
#include "au_codec.hpp"
#include "midi_codec.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace numkit::audio {

namespace {

std::string detectAudioFormat(const uint8_t *data, size_t len) {
    if (isWavBytes(data, len)) return "wav";
    if (isFlacBytes(data, len)) return "flac";
    if (isMp3Bytes(data, len)) return "mp3";
    if (isAiffBytes(data, len)) return "aiff";
    if (isAuBytes(data, len)) return "au";
    if (isMidiBytes(data, len)) return "midi";
    return "";
}

std::string extensionFromPath(const std::string &path) {
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return "";
    std::string ext = path.substr(dot + 1);
    for (char &c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::vector<uint8_t> readFileDirect(const std::string &path) {
    std::ifstream is(path, std::ios::binary);
    if (!is) {
        throw Error("audioread: could not open file: " + path,
                    0, 0, "audioread", "", "numkit:audioread:fileNotFound");
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(is), {});
}

void writeFileDirect(const std::string &path, const std::vector<uint8_t> &data) {
    std::ofstream os(path, std::ios::binary);
    if (!os) {
        throw Error("audiowrite: could not create output file: " + path,
                    0, 0, "audiowrite", "", "numkit:audiowrite:fileCreateFailed");
    }
    os.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
}

} // anonymous

AudioData audioreadFromBytes(const std::uint8_t *data, std::size_t len,
                            int64_t startSample, int64_t endSample,
                            bool nativeType,
                            std::pmr::memory_resource *mr)
{
    std::string fmt = detectAudioFormat(data, len);

    if (fmt == "wav") {
        return readWav(data, len, startSample, endSample, nativeType, mr);
    } else if (fmt == "flac") {
        return readFlac(data, len, startSample, endSample, nativeType, mr);
    } else if (fmt == "mp3") {
        return readMp3(data, len, startSample, endSample, nativeType, mr);
    } else if (fmt == "aiff") {
        return readAiff(data, len, startSample, endSample, nativeType, mr);
    } else if (fmt == "au") {
        return readAu(data, len, startSample, endSample, nativeType, mr);
    }

    throw Error("audioread: unrecognized or unsupported audio file format",
                0, 0, "audioread", "", "numkit:audioread:unsupportedFormat");
}

AudioData audioreadFromBytes(const std::string &bytes,
                            int64_t startSample, int64_t endSample,
                            bool nativeType,
                            std::pmr::memory_resource *mr)
{
    return audioreadFromBytes(reinterpret_cast<const std::uint8_t *>(bytes.data()),
                             bytes.size(), startSample, endSample, nativeType, mr);
}

AudioInfo audioinfoFromBytes(const std::uint8_t *data, std::size_t len)
{
    std::string fmt = detectAudioFormat(data, len);

    if (fmt == "wav") {
        return peekWav(data, len);
    } else if (fmt == "flac") {
        return peekFlac(data, len);
    } else if (fmt == "mp3") {
        return peekMp3(data, len);
    } else if (fmt == "aiff") {
        return peekAiff(data, len);
    } else if (fmt == "au") {
        return peekAu(data, len);
    }

    throw Error("audioinfo: unrecognized or unsupported audio file format",
                0, 0, "audioinfo", "", "numkit:audioinfo:unsupportedFormat");
}

AudioInfo audioinfoFromBytes(const std::string &bytes)
{
    return audioinfoFromBytes(reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size());
}

std::vector<std::uint8_t> audiowriteToBytes(const Value &y, double sampleRate,
                                            const std::string &format,
                                            uint16_t bitsPerSample,
                                            const std::string &title,
                                            const std::string &artist,
                                            const std::string &comment)
{
    std::string fmt = format;
    for (char &c : fmt) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (fmt == "wav" || fmt == "wave") {
        return writeWavToBytes(y, sampleRate, bitsPerSample, title, artist, comment);
    } else if (fmt == "aiff" || fmt == "aif" || fmt == "aifc") {
        return writeAiffToBytes(y, sampleRate, bitsPerSample, title, artist, comment);
    } else if (fmt == "au" || fmt == "snd") {
        return writeAuToBytes(y, sampleRate, bitsPerSample, comment);
    }

    throw Error("audiowrite: unsupported format for audio encoding: '" + format + "'",
                0, 0, "audiowrite", "", "numkit:audiowrite:unsupportedFormat");
}

AudioData audioread(const std::string &path,
                    int64_t startSample, int64_t endSample,
                    bool nativeType,
                    std::pmr::memory_resource *mr)
{
    auto bytes = readFileDirect(path);
    return audioreadFromBytes(bytes.data(), bytes.size(), startSample, endSample, nativeType, mr);
}

AudioInfo audioinfo(const std::string &path)
{
    auto bytes = readFileDirect(path);
    auto info = audioinfoFromBytes(bytes.data(), bytes.size());
    info.title = path; // Default Filename in struct
    return info;
}

void audiowrite(const std::string &path, const Value &y, double sampleRate,
                uint16_t bitsPerSample,
                const std::string &title,
                const std::string &artist,
                const std::string &comment)
{
    std::string ext = extensionFromPath(path);
    if (ext.empty()) ext = "wav";
    auto bytes = audiowriteToBytes(y, sampleRate, ext, bitsPerSample, title, artist, comment);
    writeFileDirect(path, bytes);
}

} // namespace numkit::audio
