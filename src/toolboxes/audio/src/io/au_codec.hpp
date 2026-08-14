// toolboxes/audio/src/io/au_codec.hpp
//
// In-tree Sun/NeXT AU (.au, .snd) reader, writer, and peeker.
// Zero-dependency pure C++17 implementation.

#pragma once

#include "wav_codec.hpp" // For AudioData and AudioInfo

#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <vector>

namespace numkit::audio {

/// Sniff AU/SND magic bytes directly from an in-memory buffer.
bool isAuBytes(const std::uint8_t *data, std::size_t len);
bool isAuBytes(const std::string &b);

/// Decode AU/SND audio from in-memory byte buffer.
AudioData readAu(const std::uint8_t *data, std::size_t len,
                 int64_t startSample = 1, int64_t endSample = -1,
                 bool nativeType = false,
                 std::pmr::memory_resource *mr = nullptr);

/// Peek AU/SND metadata without decoding full audio stream.
AudioInfo peekAu(const std::uint8_t *data, std::size_t len);

/// Encode audio matrix `y` (N x C) to in-memory AU byte stream.
std::vector<std::uint8_t> writeAuToBytes(const Value &y, double sampleRate,
                                         uint16_t bitsPerSample = 16,
                                         const std::string &comment = "");

} // namespace numkit::audio
