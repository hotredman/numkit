// toolboxes/image/src/io/tiff_codec.hpp
//
// In-tree TIFF and BigTIFF reader, writer, and peeker.
// Zero-dependency pure C++20 implementation.

#pragma once

#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <utility>
#include <vector>

namespace numkit::image {

/// True if the byte buffer carries a TIFF magic header (II* / MM*, classic or BigTIFF).
bool isTiffBytes(const std::string &b);
bool isTiffBytes(const std::uint8_t *data, std::size_t len);

/// Read a TIFF image (single page or specific page).
Value readTiff(const std::string &path, std::pmr::memory_resource *mr);
Value readTiff(const std::string &path, std::uint32_t page, std::pmr::memory_resource *mr);
Value readTiff(std::vector<std::uint8_t> buf, std::uint32_t page, std::pmr::memory_resource *mr);

/// Two-output form `[A, map] = readTiffWithMap(...)` for indexed/palette TIFFs.
std::pair<Value, Value> readTiffWithMap(const std::string &path, std::uint32_t page, std::pmr::memory_resource *mr);
std::pair<Value, Value> readTiffWithMap(std::vector<std::uint8_t> buf, std::uint32_t page, std::pmr::memory_resource *mr);

/// Peek TIFF first-IFD metadata without decoding raster data.
void peekTiff(const std::string &path, std::uint32_t &W, std::uint32_t &H, std::uint16_t &bits, std::uint16_t &channels);
void peekTiff(const std::vector<std::uint8_t> &buf, std::uint32_t &W, std::uint32_t &H, std::uint16_t &bits, std::uint16_t &channels);

/// Count number of IFDs (pages) in a TIFF file.
std::uint32_t tiffNumPages(const std::string &path);

/// Write an image to disk or buffer as a TIFF.
void writeTiff(const Value &A, const std::string &path, const std::string &compression, bool appendMode);
std::vector<std::uint8_t> writeTiffToBytes(const Value &A, const std::string &compression, const std::vector<std::uint8_t> *existing);

} // namespace numkit::image
