// libs/image/include/numkit/image/color/color.hpp
//
// Colour space conversions. All accept either an H×W×3 image (matching
// MATLAB) or an N×3 colormap (rows = pixels, cols = channels).
// Output class:
//   - rgb2hsv / hsv2rgb / rgb2ycbcr / ycbcr2rgb / rgb2lab / lab2rgb
//     all return double in MATLAB; we follow the same convention.
//   - rgb2xyz / xyz2rgb same.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <vector>

namespace numkit::image {

/// imsplit(I) — split an H×W×P volume into P planes (H×W each).
/// For 2-D input returns a single H×W copy in planes[0]. Output
/// vector is resized to P; output planes share the input's class.
void imsplit(std::pmr::memory_resource *mr,
             const Value &I, std::vector<Value> &planes);

Value rgb2hsv  (std::pmr::memory_resource *mr, const Value &x);
Value hsv2rgb  (std::pmr::memory_resource *mr, const Value &x);
Value rgb2ycbcr(std::pmr::memory_resource *mr, const Value &x);
Value ycbcr2rgb(std::pmr::memory_resource *mr, const Value &x);

Value rgb2xyz  (std::pmr::memory_resource *mr, const Value &x);
Value xyz2rgb  (std::pmr::memory_resource *mr, const Value &x);
Value rgb2lab  (std::pmr::memory_resource *mr, const Value &x);
Value lab2rgb  (std::pmr::memory_resource *mr, const Value &x);
Value xyz2lab  (std::pmr::memory_resource *mr, const Value &x);
Value lab2xyz  (std::pmr::memory_resource *mr, const Value &x);

/// colorangle(rgb1, rgb2) — angle in degrees between two RGB
/// colours: rad2deg(acos(dot(rgb1, rgb2) / (|rgb1|·|rgb2|))).
/// Inputs may be 3-element vectors (any orientation) or N×3
/// matrices; broadcasting between a single colour and an N×3
/// stack is supported. Returns 0 when both colours are zero, NaN
/// when only one is zero. The cosine is clamped to [−1, 1] to
/// guard against floating-point drift on identical colours.
Value colorangle(std::pmr::memory_resource *mr,
                 const Value &rgb1, const Value &rgb2);

/// label2rgb(L, cmap [, background]) — colourise a labelled image.
/// `L` is an H×W non-negative integer-valued matrix. `cmap` is an
/// N×3 colormap (double in [0, 1]). Pixels with label == 0 take the
/// `background` color (default [1, 1, 1] = white). Output is H×W×3
/// uint8.
///
/// The full MATLAB signature accepts a colormap-name string or a
/// function handle for `cmap`; both require a `jet` / `hsv` / etc.
/// generator that we don't expose yet, so callers must pass an
/// explicit N×3 matrix here.
Value label2rgb(std::pmr::memory_resource *mr,
                const Value &L, const Value &cmap,
                const Value &background);

} // namespace numkit::image
