// toolboxes/audio/src/io/wav_codec.hpp
//
// In-tree RIFF/WAVE reader, writer, and peeker.
// Zero-dependency pure C++17 implementation.

#pragma once

#include <numkit/audio/io/audio_io.hpp>

namespace numkit::audio {

/// Sniff RIFF/WAVE header directly from an in-memory buffer.
bool isWavBytes(const std::uint8_t *data, std::size_t len);
bool isWavBytes(const std::string &b);

/// Decode WAV audio from in-memory byte buffer.
/// @param startSample 1-based start sample index (inclusive). 1 = beginning.
/// @param endSample   1-based end sample index (inclusive). -1 = end of stream.
/// @param nativeType  If true, returns native integer/float ValueType instead of double.
AudioData readWav(const std::uint8_t *data, std::size_t len,
                  int64_t startSample = 1, int64_t endSample = -1,
                  bool nativeType = false,
                  std::pmr::memory_resource *mr = nullptr);

/// Peek WAV metadata without decoding sample data.
AudioInfo peekWav(const std::uint8_t *data, std::size_t len);

/// Encode audio matrix `y` (N x C) to in-memory WAV byte stream.
/// @param bitsPerSample 8, 16, 24, or 32 (32-bit floats encoded as IEEE Float format 3).
std::vector<std::uint8_t> writeWavToBytes(const Value &y, double sampleRate,
                                          uint16_t bitsPerSample = 16,
                                          const std::string &title = "",
                                          const std::string &artist = "",
                                          const std::string &comment = "");

} // namespace numkit::audio
