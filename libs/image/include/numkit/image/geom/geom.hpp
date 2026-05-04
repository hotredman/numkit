// libs/image/include/numkit/image/geom/geom.hpp
//
// Basic geometric transforms: resize / crop / rotate / translate.
// All preserve the input element type (uint8, double, …) and accept
// either H×W (grayscale) or H×W×C (multi-channel) arrays.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::image {

/// `B = imresize(A, scale)` or `imresize(A, [outH outW])` —
/// resample to a new size. method = "nearest" | "bilinear" (default).
Value imresize(std::pmr::memory_resource *mr,
               const Value &A, double scale,
               const std::string &method);
Value imresize(std::pmr::memory_resource *mr,
               const Value &A, size_t outH, size_t outW,
               const std::string &method);

/// `B = imcrop(A, [xmin ymin width height])` — return the rectangle
/// (1-based, MATLAB style). Coordinates clamped to the image bounds.
Value imcrop(std::pmr::memory_resource *mr,
             const Value &A, double xmin, double ymin,
             double width, double height);

/// `B = imrotate(A, angle [, method [, bbox]])` — rotate CCW by
/// `angle` degrees. method = "nearest" | "bilinear" (default).
/// bbox = "loose" (default, expand to fit rotated extent) | "crop"
/// (keep input dims). Out-of-source pixels filled with 0.
Value imrotate(std::pmr::memory_resource *mr,
               const Value &A, double angle,
               const std::string &method, const std::string &bbox);

/// `B = imtranslate(A, [dx dy])` — shift the image by (dx, dy).
/// Same dims as input; out-of-source pixels filled with 0.
/// Half-pixel shifts use bilinear interpolation.
Value imtranslate(std::pmr::memory_resource *mr,
                  const Value &A, double dx, double dy);

/// `pix = axes2pix(n, extent, axesCoord)` — convert world-axis
/// coordinates to intrinsic pixel coordinates (1-based) for an
/// `n`-row-or-col image whose first / last pixel centers lie at
/// `extent(1)` / `extent(end)`. Degenerate cases (n=1 or
/// extent(1)==extent(end)) collapse the mapping to a translation.
/// Output keeps the shape of `axesCoord`.
Value axes2pix(std::pmr::memory_resource *mr,
               double n, const Value &extent, const Value &axesCoord);

/// `B = impyramid(A, type)` — Burt-Adelson 2-D pyramid step.
/// type = "reduce" → output ceil(M/2)×ceil(N/2) after low-pass filtering
/// type = "expand" → output (2M-1)×(2N-1) after zero-stuffing + filter
/// 5-tap separable kernel [0.05 0.25 0.4 0.25 0.05] with replicate
/// boundary; 3-D inputs processed per-channel.
Value impyramid(std::pmr::memory_resource *mr,
                const Value &A, const std::string &type);

} // namespace numkit::image
