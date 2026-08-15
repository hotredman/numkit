// toolboxes/image/include/numkit/image/io/io.hpp
//
// Autonomous, zero-dependency in-tree image disk I/O (PNG, JPEG, BMP,
// TGA, PNM, TIFF/BigTIFF).
//
// imread returns an H × W (grayscale) or H × W × 3 / 4 (color)
// uint8 or uint16 array, preserving full scientific 16-bit depth.

#pragma once

#include <cstdint>
#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <utility>
#include <vector>

namespace numkit::image {

/// Read an image from disk (`A = imread(path)`).
///
/// Decoded element type is uint8 or uint16 (preserving native bit depth).
/// Channel count depends on the source file:
///   - grayscale → H×W (uint8 or uint16).
///   - RGB       → H×W×3 (channels R, G, B).
///   - RGBA      → H×W×4 (channels R, G, B, A).
///
/// @param path  Filesystem path (relative or absolute).
/// @param mr    Memory resource (nullptr → process default).
/// @return      uint8 image Value.
/// @throws      Error on missing file or unsupported / corrupt format.
///
/// @see imwrite, imfinfo
Value imread(const std::string &path,
             std::pmr::memory_resource *mr = nullptr);

/// Write an image to disk (`imwrite(A, path)`).
///
/// Encodes an H×W or H×W×{1, 3, 4} uint8 array. Format inferred from
/// the path extension:
///
///   | Extension       | Format       |
///   | --------------- | ------------ |
///   | `.png`          | PNG          |
///   | `.bmp`          | BMP          |
///   | `.tga`          | TGA          |
///   | `.jpg`, `.jpeg` | JPEG (q=90)  |
///
/// @param A     Image (uint8, 1/3/4 channels).
/// @param path  Output path; extension picks the encoder.
/// @param mr    Memory resource (nullptr → process default).
/// @throws      Error on unsupported extension or write failure.
///
/// @see imread
void imwrite(const Value &A, const std::string &path,
             std::pmr::memory_resource *mr = nullptr);

/// Encode an image to in-memory bytes (companion to imwrite). Used by the
/// engine entry so writes go through the VFS — works on the virtual AND
/// real filesystem. `ext` is the lowercase extension WITHOUT a dot
/// ("png" | "bmp" | "tga" | "jpg" | "jpeg"). TIFF is handled separately by
/// writeTiffToBytes().
///
/// @throws Error on unsupported extension or encode failure.
std::string imwriteToBytes(const Value &A, const std::string &ext,
                           std::pmr::memory_resource *mr = nullptr);

/// Read image header metadata without decoding pixels (`S = imfinfo(path)`).
///
/// Returns a struct with the fields:
///   - `Filename`         — path passed in.
///   - `Format`           — `"png"|"jpg"|"bmp"|"gif"|"pnm"|"hdr"|"psd"|"tga"`.
///   - `Width`, `Height`  — image dimensions in pixels.
///   - `NumberOfChannels` — 1, 3, or 4.
///   - `NumberOfSamples`  — 1, 3, or 4 (MATLAB standard alias for channels).
///   - `ColorType`        — `"grayscale"|"truecolor"|"truecoloralpha"`.
///   - `FileSize`         — bytes on disk.
///
/// @param path  Filesystem path.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Struct Value with the metadata fields above.
///
/// @see imread
Value imfinfo(const std::string &path,
              std::pmr::memory_resource *mr = nullptr);

/// imfinfo from already-loaded bytes (the engine entry feeds these from the
/// VFS so it works on the virtual + real filesystem). `filename` is recorded
/// in the returned struct's Filename field but is not opened.
///
/// @see imfinfo
Value imfinfoFromBytes(const std::string &bytes, const std::string &filename,
                       std::pmr::memory_resource *mr = nullptr);

/// Minimal TIFF reader for the formats stb doesn't decode.
///
/// Implements the **baseline uncompressed** subset of TIFF 6.0:
///   - Compression = 1 (none) only
///   - Photometric = 1 (BlackIsZero gray) or 2 (RGB)
///   - SamplesPerPixel ∈ {1, 3, 4}; BitsPerSample ∈ {8, 16}
///   - PlanarConfiguration = 1 (chunky)
///   - Single-page (first IFD)
///   - Both byte orders (II little-endian, MM big-endian)
///
/// Throws a clear error for any branch outside this set (compression
/// schemes, planar layout, palette / CMYK, multi-page, float / int
/// samples). Future cycles will widen the coverage.
///
/// @param path  Filesystem path.
/// @param mr    Memory resource (nullptr → process default).
/// @return      H × W uint8 / uint16 (gray) or H × W × {3, 4} uint8 / uint16.
///
/// @see imread
Value readTiff(const std::string &path,
               std::pmr::memory_resource *mr = nullptr);

/// Multi-page form: read the `page`-th IFD (1-based). MATLAB's
/// `imread(file, k)` semantics.
Value readTiff(const std::string &path, std::uint32_t page,
               std::pmr::memory_resource *mr = nullptr);

/// Buffer overloads — decode TIFF from already-loaded bytes (the IDE feeds
/// these from the VFS so reads work on the virtual + real filesystem).
Value readTiff(std::vector<std::uint8_t> buf, std::uint32_t page,
               std::pmr::memory_resource *mr = nullptr);
std::pair<Value, Value>
readTiffWithMap(std::vector<std::uint8_t> buf, std::uint32_t page,
                std::pmr::memory_resource *mr = nullptr);

/// Two-output read for palette TIFFs: returns `(indices, colormap)`.
/// `indices` is the same as `readTiff` (uint8 / uint16). `colormap` is
/// K×3 DOUBLE in [0, 1] for palette files (Photometric = 3), or empty
/// for non-palette files. K = 2^BitsPerSample. Matches MATLAB's
/// `[A, map] = imread(file)` for indexed images.
std::pair<Value, Value>
readTiffWithMap(const std::string &path, std::uint32_t page,
                std::pmr::memory_resource *mr = nullptr);

/// Peek a TIFF file's first-IFD metadata without decoding pixels.
/// Output struct: `Width`, `Height`, `BitsPerSample`, `NumberOfChannels`.
///
/// @see imfinfo
void peekTiff(const std::string &path,
              std::uint32_t &W,
              std::uint32_t &H,
              std::uint16_t &bits,
              std::uint16_t &channels);

/// Buffer overload — peek metadata from already-loaded TIFF bytes (the IDE
/// feeds these from the VFS so imfinfo works on the virtual + real FS).
void peekTiff(const std::vector<std::uint8_t> &buf,
              std::uint32_t &W,
              std::uint32_t &H,
              std::uint16_t &bits,
              std::uint16_t &channels);

/// Count the number of IFDs (pages) in a TIFF file. Walks the IFD chain
/// without decoding any pixel data.
std::uint32_t tiffNumPages(const std::string &path);

/// Write `A` to `path` as a baseline TIFF. Companion to readTiff().
///
/// Supports uint8 / uint16 inputs (other numeric types are clamped to
/// uint8). Channels: 1 (gray), 3 (RGB), 4 (RGBA). Compression options:
/// `"none"`, `"packbits"`, `"lzw"`, `"deflate"` (Deflate requires
/// NUMKIT_WITH_ZLIB at build time).
///
/// @param A            Image (uint8 / uint16; 1, 3, or 4 channels).
/// @param path         Output path.
/// @param compression  Compression scheme name.
/// @param appendMode   If true and the file already exists, append a
///                     new IFD; otherwise overwrite.
void writeTiff(const Value &A, const std::string &path,
               const std::string &compression = "none",
               bool appendMode = false);

/// Buffer form — encode `A` to a TIFF byte vector (the engine entry writes
/// these through the VFS). When `existing` is non-null and non-empty, a new
/// IFD is appended to those bytes (multi-page); otherwise a fresh
/// single-page file is produced.
///
/// @param A            Image (uint8 / uint16; 1, 3, or 4 channels).
/// @param compression  "none" | "packbits" | "lzw" | "deflate".
/// @param existing     Optional bytes of a prior TIFF to append a page to.
std::vector<std::uint8_t>
writeTiffToBytes(const Value &A,
                 const std::string &compression = "none",
                 const std::vector<std::uint8_t> *existing = nullptr);

} // namespace numkit::image
