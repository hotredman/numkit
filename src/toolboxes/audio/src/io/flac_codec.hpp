// toolboxes/audio/src/io/flac_codec.hpp
//
// In-tree FLAC (Free Lossless Audio Codec) reader and peeker.
// Zero-dependency pure C++17 implementation.

#pragma once

#include "wav_codec.hpp" // For AudioData and AudioInfo

#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <vector>

namespace numkit::audio {

/// Sniff FLAC magic bytes directly from an in-memory buffer.
bool isFlacBytes(const std::uint8_t *data, std::size_t len);
bool isFlacBytes(const std::string &b);

/// Decode FLAC audio from in-memory byte buffer.
AudioData readFlac(const std::uint8_t *data, std::size_t len,
                   int64_t startSample = 1, int64_t endSample = -1,
                   bool nativeType = false,
                   std::pmr::memory_resource *mr = nullptr);

/// Peek FLAC metadata without decoding all audio frames.
AudioInfo peekFlac(const std::uint8_t *data, std::size_t len);

} // namespace numkit::audio
