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

Value rgb2ntsc (std::pmr::memory_resource *mr, const Value &x);
Value ntsc2rgb (std::pmr::memory_resource *mr, const Value &x);

Value rgb2xyz  (std::pmr::memory_resource *mr, const Value &x);
Value xyz2rgb  (std::pmr::memory_resource *mr, const Value &x);
Value rgb2lab  (std::pmr::memory_resource *mr, const Value &x);
Value lab2rgb  (std::pmr::memory_resource *mr, const Value &x);
Value xyz2lab  (std::pmr::memory_resource *mr, const Value &x);
Value lab2xyz  (std::pmr::memory_resource *mr, const Value &x);

/// L*a*b* class conversion: L* in [0, 100] (or 0..255 / 0..65280
/// integer), a*/b* in [-128, 127] (or [0, 65280] for uint16). The
/// helpers below dispatch on input class and rescale + offset
/// per-channel; output keeps the input's shape (Mx3 colormap or
/// MxNx3 image).
Value lab2double (std::pmr::memory_resource *mr, const Value &lab);
Value lab2single (std::pmr::memory_resource *mr, const Value &lab);
Value lab2uint8  (std::pmr::memory_resource *mr, const Value &lab);
Value lab2uint16 (std::pmr::memory_resource *mr, const Value &lab);

/// `M = colorgradient(C [, w] [, n])` — colormap that smoothly
/// traverses the K-by-3 anchor RGB colors `C` with relative segment
/// weights `w` (length K-1, default ones) into `n` output rows
/// (default 64). Per-segment linspace; output is double N×3.
Value colorgradient(std::pmr::memory_resource *mr,
                    const Value &C, const Value &w, int n);

/// `RGB = wavelength2rgb(wavelength [, class [, gamma]])` —
/// piecewise visible-light wavelength → RGB mapping (Bruton 1996).
/// Scalar input returns a 1×3 row; 1-D vector returns
/// 1×N×3 / N×1×3; 2-D matrix returns H×W×3. `class` defaults to
/// "double" and `gamma` to 0.8. Output class follows `class`
/// through the existing im2* helpers.
Value wavelength2rgb(std::pmr::memory_resource *mr,
                     const Value &wavelength,
                     const std::string &out_class,
                     double gamma);

/// colorangle(rgb1, rgb2) — angle in degrees between two RGB
/// colours: rad2deg(acos(dot(rgb1, rgb2) / (|rgb1|·|rgb2|))).
/// Inputs may be 3-element vectors (any orientation) or N×3
/// matrices; broadcasting between a single colour and an N×3
/// stack is supported. Returns 0 when both colours are zero, NaN
/// when only one is zero. The cosine is clamped to [−1, 1] to
/// guard against floating-point drift on identical colours.
Value colorangle(std::pmr::memory_resource *mr,
                 const Value &rgb1, const Value &rgb2);

/// `map = gray([n])` — N×3 grayscale colormap. Default n = 256 (we
/// don't track figure state). n == 1 → [0, 0, 0]; n ≤ 0 → 0×3.
/// Otherwise gr = (0:n-1)/(n-1), repeated across all 3 channels.
Value gray_cmap(std::pmr::memory_resource *mr, int n);

/// `gmap = cmap2gray(cmap)` — colormap → grayscale colormap.
/// Input is an N×3 RGB colormap with values in [0, 1] (any numeric
/// class accepted; coerced to double). Output is an N×1 column of
/// luminance values via the Octave-image weights
/// (0.298936, 0.587043, 0.114021). MATLAB R2020b+; Octave-image has
/// it under the same name.
Value cmap2gray(std::pmr::memory_resource *mr, const Value &cmap);

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
