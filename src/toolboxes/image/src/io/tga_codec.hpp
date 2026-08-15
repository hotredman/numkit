// toolboxes/image/src/io/tga_codec.hpp
//
// In-tree Truevision TGA (.tga) decoder and encoder. Zero external dependencies.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::image {

/// Decode a TGA image from an in-memory byte buffer.
/// @return H×W uint8 (grayscale) or H×W×3/4 uint8 (truecolor RGB/RGBA).
Value readTga(const std::uint8_t *data, std::size_t len,
              std::pmr::memory_resource *mr = nullptr);

/// Encode an image Value to TGA format bytes.
/// @param A Image (H×W grayscale, H×W×3 RGB, or H×W×4 RGBA uint8).
std::string writeTgaToBytes(const Value &A);

/// Inspect a TGA header without decoding all pixels.
bool peekTga(const std::uint8_t *data, std::size_t len,
             std::uint32_t &W, std::uint32_t &H,
             std::uint16_t &bitsPerSample, std::uint16_t &channels);

} // namespace numkit::image
