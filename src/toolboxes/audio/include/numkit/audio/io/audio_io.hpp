/// @file audio_io.hpp
/// @ingroup group_audio
// toolboxes/audio/include/numkit/audio/io/audio_io.hpp
//
// Unified Audio I/O facade for NumKit: audioread, audiowrite, audioinfo.
// Zero-dependency pure C++17 implementation supporting WAV, FLAC, MP3, AIFF, AU, MIDI.

#pragma once

#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <vector>

namespace numkit::audio {

struct AudioData {
    Value y;                      // N x C matrix (samples along rows, channels along cols)
    double sampleRate = 0.0;      // Fs in Hz
    uint16_t bitsPerSample = 16;
    uint16_t numChannels = 1;
    uint64_t totalSamples = 0;    // Number of frames per channel
    std::string title;
    std::string artist;
    std::string comment;
};

struct AudioInfo {
    std::string format;
    std::string compressionMethod;
    uint32_t numChannels = 0;
    double sampleRate = 0.0;
    uint64_t totalSamples = 0;
    double duration = 0.0;
    uint16_t bitsPerSample = 0;
    uint32_t bitRate = 0;         // in kbps
    std::string title;
    std::string artist;
    std::string comment;
};

// In-Memory Byte-Oriented Public APIs (WASM / VFS Friendly)

/// Decode audio from memory buffer, automatically sniffing format (WAV, FLAC, MP3, AIFF, AU).
AudioData audioreadFromBytes(const std::uint8_t *data, std::size_t len,
                            int64_t startSample = 1, int64_t endSample = -1,
                            bool nativeType = false,
                            std::pmr::memory_resource *mr = nullptr);

AudioData audioreadFromBytes(const std::string &bytes,
                            int64_t startSample = 1, int64_t endSample = -1,
                            bool nativeType = false,
                            std::pmr::memory_resource *mr = nullptr);

/// Peek audio file metadata without decoding all audio samples.
AudioInfo audioinfoFromBytes(const std::uint8_t *data, std::size_t len);
AudioInfo audioinfoFromBytes(const std::string &bytes);

/// Encode audio matrix `y` (N x C) into memory buffer (format: "wav", "aiff", "au").
std::vector<std::uint8_t> audiowriteToBytes(const Value &y, double sampleRate,
                                            const std::string &format = "wav",
                                            uint16_t bitsPerSample = 16,
                                            const std::string &title = "",
                                            const std::string &artist = "",
                                            const std::string &comment = "");

// File-System Public APIs (Native / OS wrappers)

AudioData audioread(const std::string &path,
                    int64_t startSample = 1, int64_t endSample = -1,
                    bool nativeType = false,
                    std::pmr::memory_resource *mr = nullptr);

AudioInfo audioinfo(const std::string &path);

void audiowrite(const std::string &path, const Value &y, double sampleRate,
                uint16_t bitsPerSample = 16,
                const std::string &title = "",
                const std::string &artist = "",
                const std::string &comment = "");

} // namespace numkit::audio
