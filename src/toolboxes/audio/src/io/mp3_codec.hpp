// toolboxes/audio/src/io/mp3_codec.hpp
//
// In-tree MP3 (MPEG-1/2/2.5 Audio Layer III) reader and peeker.
// Zero-dependency pure C++17 implementation.

#pragma once

#include "wav_codec.hpp" // For AudioData and AudioInfo

#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <vector>

namespace numkit::audio {

/// Sniff MP3 magic or frame sync directly from an in-memory buffer.
bool isMp3Bytes(const std::uint8_t *data, std::size_t len);
bool isMp3Bytes(const std::string &b);

/// Decode MP3 audio from in-memory byte buffer.
AudioData readMp3(const std::uint8_t *data, std::size_t len,
                  int64_t startSample = 1, int64_t endSample = -1,
                  bool nativeType = false,
                  std::pmr::memory_resource *mr = nullptr);

/// Peek MP3 metadata and ID3 tags without decoding full stream.
AudioInfo peekMp3(const std::uint8_t *data, std::size_t len);

} // namespace numkit::audio
