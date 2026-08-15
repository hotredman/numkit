// toolboxes/image/src/io/jpeg_codec.hpp
//
// In-tree Baseline JPEG (.jpg, .jpeg) decoder and encoder.
// Zero external dependencies.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::image {

/// Decode a JPEG image from an in-memory byte buffer.
/// @return H×W uint8 (grayscale) or H×W×3 uint8 (truecolor RGB).
Value readJpeg(const std::uint8_t *data, std::size_t len,
               std::pmr::memory_resource *mr = nullptr);

/// Encode an image Value to JPEG format bytes.
/// @param A        Image (uint8; H×W grayscale or H×W×3 RGB).
/// @param quality  Quality level (1..100, default 90).
std::string writeJpegToBytes(const Value &A, int quality = 90);

/// Inspect a JPEG header without decoding pixels.
bool peekJpeg(const std::uint8_t *data, std::size_t len,
              std::uint32_t &W, std::uint32_t &H,
              std::uint16_t &bitsPerSample, std::uint16_t &channels);

} // namespace numkit::image
