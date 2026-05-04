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

/// `imread(path)` — decode an image file from disk. Returns:
///   gray  → H×W   uint8
///   RGB   → H×W×3 uint8 (channels = R, G, B)
///   RGBA  → H×W×4 uint8 (channels = R, G, B, A)
/// Throws Error on missing file or unsupported / corrupt format.
Value imread(std::pmr::memory_resource *mr, const std::string &path);

/// `imwrite(A, path)` — encode an H×W or H×W×{1,3,4} uint8 array to
/// disk. Format inferred from the path extension: .png / .bmp / .tga
/// / .jpg|.jpeg. Returns nothing.
void imwrite(std::pmr::memory_resource *mr,
             const Value &A, const std::string &path);

/// `imfinfo(path)` — read header metadata without decoding pixels.
/// Returns a struct with fields:
///   Filename, Format ('png'|'jpg'|'bmp'|'gif'|'pnm'|'hdr'|'psd'|'tga'),
///   Width, Height, NumberOfChannels (1/3/4), ColorType
///   ('grayscale'|'truecolor'|'truecoloralpha'), FileSize.
Value imfinfo(std::pmr::memory_resource *mr, const std::string &path);

} // namespace numkit::image
