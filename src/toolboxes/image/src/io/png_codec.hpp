// toolboxes/image/src/io/png_codec.hpp
//
// In-tree Portable Network Graphics (.png) decoder and encoder.
// Full 8-bit and 16-bit support with zero external dependencies.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <utility>

namespace numkit::image {

/// Decode a PNG image from an in-memory byte buffer.
/// @return H×W uint8/uint16 (gray), H×W×3 uint8/uint16 (RGB), or H×W×4 uint8/uint16 (RGBA).
Value readPng(const std::uint8_t *data, std::size_t len,
              std::pmr::memory_resource *mr = nullptr);

/// Two-output form for palette PNG images (`[A, map] = imread(...)`).
/// If image is not indexed, colormap will be empty.
std::pair<Value, Value>
readPngWithMap(const std::uint8_t *data, std::size_t len,
               std::pmr::memory_resource *mr = nullptr);

/// Encode an image Value to PNG format bytes.
/// @param A      Image (uint8/uint16; 1, 3, or 4 channels).
/// @param level  Compression level (0..9, default 6).
std::string writePngToBytes(const Value &A, int level = 6);

/// Inspect a PNG header without decoding pixel data.
bool peekPng(const std::uint8_t *data, std::size_t len,
             std::uint32_t &W, std::uint32_t &H,
             std::uint16_t &bitsPerSample, std::uint16_t &channels);

} // namespace numkit::image
