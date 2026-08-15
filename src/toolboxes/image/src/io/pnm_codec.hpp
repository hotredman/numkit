// toolboxes/image/src/io/pnm_codec.hpp
//
// In-tree Netpbm (.pnm, .ppm, .pgm, .pbm) decoder and encoder. Zero external dependencies.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::image {

/// Decode a PNM/PGM/PPM/PBM image from an in-memory byte buffer.
/// @return H×W uint8/uint16 (grayscale) or H×W×3 uint8/uint16 (RGB).
Value readPnm(const std::uint8_t *data, std::size_t len,
              std::pmr::memory_resource *mr = nullptr);

/// Encode an image Value to PNM (PGM/PPM) format bytes.
/// @param A Image (H×W grayscale or H×W×3 RGB uint8/uint16).
std::string writePnmToBytes(const Value &A, const std::string &ext = "ppm");

/// Inspect a PNM header without decoding all pixels.
bool peekPnm(const std::uint8_t *data, std::size_t len,
             std::uint32_t &W, std::uint32_t &H,
             std::uint16_t &bitsPerSample, std::uint16_t &channels);

} // namespace numkit::image
