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

/// @addtogroup group_audio
/// @{


/// @brief Decoded audio waveform data and basic metadata.
struct AudioData {
    Value y;                      ///< Audio data matrix `N x C` (samples along rows, channels along columns).
    double sampleRate = 0.0;      ///< Sampling frequency `Fs` in Hz.
    uint16_t bitsPerSample = 16;  ///< Quantization bit depth per sample.
    uint16_t numChannels = 1;     ///< Number of interleaved audio channels.
    uint64_t totalSamples = 0;    ///< Total number of sample frames per channel.
    std::string title;            ///< Embedded track title metadata.
    std::string artist;           ///< Embedded artist metadata.
    std::string comment;          ///< Embedded comment or description metadata.
};

/// @brief Audio file metadata container (`info = audioinfo(filename)`).
struct AudioInfo {
    std::string format;            ///< Container format (e.g. `"WAV"`, `"FLAC"`, `"MP3"`, `"AIFF"`).
    std::string compressionMethod; ///< Audio compression codec name.
    uint32_t numChannels = 0;      ///< Number of audio channels.
    double sampleRate = 0.0;       ///< Sampling rate in Hz.
    uint64_t totalSamples = 0;     ///< Total sample frames in file.
    double duration = 0.0;         ///< Total duration in seconds.
    uint16_t bitsPerSample = 0;    ///< Bits per sample.
    uint32_t bitRate = 0;          ///< Bitrate in kbps.
    std::string title;             ///< Track title.
    std::string artist;            ///< Track artist.
    std::string comment;           ///< Comment or genre.
};

// ── In-Memory Byte-Oriented Public APIs (WASM / VFS Friendly) ────────────────

/// @brief Decodes audio from raw memory buffer, sniffing format automatically (`[y, Fs] = audioread(bytes)`).
/// @param data Pointer to raw audio file bytes.
/// @param len Length of buffer in bytes.
/// @param startSample 1-based start sample index.
/// @param endSample 1-based end sample index (-1 for end of stream).
/// @param nativeType True to return integer types, false for normalized `[-1.0, 1.0]` double.
/// @param mr Memory resource for output allocation.
/// @return AudioData containing audio matrix and sampling rate.
AudioData audioreadFromBytes(const std::uint8_t *data, std::size_t len,
                            int64_t startSample = 1, int64_t endSample = -1,
                            bool nativeType = false,
                            std::pmr::memory_resource *mr = nullptr);

/// @brief Decodes audio from string memory buffer (`audioread(bytes)`).
/// @param bytes Byte string containing audio file contents.
/// @param startSample 1-based start sample index.
/// @param endSample 1-based end sample index (-1 for end of stream).
/// @param nativeType True to return integer types, false for normalized `[-1.0, 1.0]` double.
/// @param mr Memory resource for output allocation.
/// @return AudioData containing audio matrix and sampling rate.
AudioData audioreadFromBytes(const std::string &bytes,
                            int64_t startSample = 1, int64_t endSample = -1,
                            bool nativeType = false,
                            std::pmr::memory_resource *mr = nullptr);

/// @brief Reads audio metadata from memory buffer without decoding audio samples (`info = audioinfo(bytes)`).
/// @param data Pointer to raw audio bytes.
/// @param len Length of buffer in bytes.
/// @return AudioInfo containing file format, sample rate, channels, and duration.
AudioInfo audioinfoFromBytes(const std::uint8_t *data, std::size_t len);

/// @brief Reads audio metadata from string memory buffer (`info = audioinfo(bytes)`).
/// @param bytes Audio file bytes as string.
/// @return AudioInfo metadata struct.
AudioInfo audioinfoFromBytes(const std::string &bytes);

/// @brief Encodes audio matrix `y` into memory byte buffer (`bytes = audiowrite(y, Fs, ...)`).
/// @param y Audio sample matrix `N x C`.
/// @param sampleRate Sampling rate in Hz.
/// @param format Audio format string (`"wav"`, `"aiff"`, `"au"`).
/// @param bitsPerSample Bit depth (16, 24, 32).
/// @param title Optional metadata title.
/// @param artist Optional metadata artist.
/// @param comment Optional metadata comment.
/// @return Vector of encoded file bytes.
std::vector<std::uint8_t> audiowriteToBytes(const Value &y, double sampleRate,
                                            const std::string &format = "wav",
                                            uint16_t bitsPerSample = 16,
                                            const std::string &title = "",
                                            const std::string &artist = "",
                                            const std::string &comment = "");

// ── File-System Public APIs (Native / OS wrappers) ──────────────────────────

/// @brief Reads audio file from filesystem (`[y, Fs] = audioread(filename)`).
/// @param path Path to audio file.
/// @param startSample 1-based start frame index (default: 1).
/// @param endSample 1-based end frame index (default: -1 for all).
/// @param nativeType True to return integer types, false for normalized `[-1.0, 1.0]` double.
/// @param mr Memory resource for output allocation.
/// @return AudioData containing decoded samples matrix and metadata.
/// @see audiowrite, audioinfo
AudioData audioread(const std::string &path,
                    int64_t startSample = 1, int64_t endSample = -1,
                    bool nativeType = false,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Returns audio file metadata (`info = audioinfo(filename)`).
/// @param path Path to audio file.
/// @return AudioInfo struct containing format, channels, sample rate, and duration.
/// @see audioread, audiowrite
AudioInfo audioinfo(const std::string &path);

/// @brief Writes audio data matrix `y` to file on disk (`audiowrite(filename, y, Fs)`).
/// @param path Path to output audio file (e.g. `.wav`, `.aiff`, `.au`).
/// @param y Audio matrix `N x C`.
/// @param sampleRate Sampling rate in Hz.
/// @param bitsPerSample Quantization bit depth (default 16).
/// @param title Optional track title metadata.
/// @param artist Optional artist metadata.
/// @param comment Optional comment metadata.
/// @see audioread, audioinfo
void audiowrite(const std::string &path, const Value &y, double sampleRate,
                uint16_t bitsPerSample = 16,
                const std::string &title = "",
                const std::string &artist = "",
                const std::string &comment = "");


/// @}
} // namespace numkit::audio
