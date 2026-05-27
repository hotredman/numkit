// libs/image/include/numkit/image/io/io.hpp
//
// Image disk I/O. Backed by stb_image (decode) and stb_image_write
// (encode) — single-header public-domain libs vendored under
// third_party/stb/. Supported formats: PNG, JPG/JPEG, BMP, TGA,
// PSD, GIF (decode only), HDR/PIC, PNM.
//
// imread returns an H × W (grayscale) or H × W × 3 / 4 (color)
// uint8 array. We don't expose the depth-16 readers here yet; PNG-16
// is decoded and 8-bit-truncated by stb's default API.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::image {

/// Read an image from disk (`A = imread(path)`).
///
/// Decoded element type is always uint8. Channel count depends on
/// the source file:
///   - grayscale → H×W uint8.
///   - RGB       → H×W×3 uint8 (channels R, G, B).
///   - RGBA      → H×W×4 uint8 (channels R, G, B, A).
///
/// PNG-16 images are decoded and 8-bit-truncated by stb's default
/// API; full 16-bit decoders are not yet exposed.
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

/// Read image header metadata without decoding pixels (`S = imfinfo(path)`).
///
/// Returns a struct with the fields:
///   - `Filename`         — path passed in.
///   - `Format`           — `"png"|"jpg"|"bmp"|"gif"|"pnm"|"hdr"|"psd"|"tga"`.
///   - `Width`, `Height`  — image dimensions in pixels.
///   - `NumberOfChannels` — 1, 3, or 4.
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

/// Peek a TIFF file's first-IFD metadata without decoding pixels.
/// Output struct: `Width`, `Height`, `BitsPerSample`, `NumberOfChannels`.
/// Throws on malformed TIFFs; baseline-only (same coverage as readTiff).
///
/// @see imfinfo
void peekTiff(const std::string &path,
              std::uint32_t &W,
              std::uint32_t &H,
              std::uint16_t &bits,
              std::uint16_t &channels);

} // namespace numkit::image
