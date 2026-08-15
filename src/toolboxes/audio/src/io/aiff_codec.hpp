// toolboxes/audio/src/io/aiff_codec.hpp
//
// In-tree AIFF / AIFF-C (Apple / Big-Endian PCM) reader, writer, and peeker.
// Zero-dependency pure C++17 implementation.

#pragma once

#include "wav_codec.hpp" // For AudioData and AudioInfo

#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <vector>

namespace numkit::audio {

/// Sniff AIFF magic bytes directly from an in-memory buffer.
bool isAiffBytes(const std::uint8_t *data, std::size_t len);
bool isAiffBytes(const std::string &b);

/// Decode AIFF audio from in-memory byte buffer.
AudioData readAiff(const std::uint8_t *data, std::size_t len,
                   int64_t startSample = 1, int64_t endSample = -1,
                   bool nativeType = false,
                   std::pmr::memory_resource *mr = nullptr);

/// Peek AIFF metadata without decoding full audio stream.
AudioInfo peekAiff(const std::uint8_t *data, std::size_t len);

/// Encode audio matrix `y` (N x C) to in-memory AIFF byte stream.
std::vector<std::uint8_t> writeAiffToBytes(const Value &y, double sampleRate,
                                           uint16_t bitsPerSample = 16,
                                           const std::string &title = "",
                                           const std::string &artist = "",
                                           const std::string &comment = "");

} // namespace numkit::audio
