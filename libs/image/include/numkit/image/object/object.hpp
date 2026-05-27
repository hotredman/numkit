// libs/image/include/numkit/image/object/object.hpp
//
// Object analysis: gradient / edge detection.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::image {

/// Component gradients (`[Gx, Gy] = imgradientxy(I, method)`).
///
/// Computes the horizontal and vertical first-order partial
/// derivatives of `I` using one of:
///
///   | method           | kernel                                  |
///   | ---------------- | --------------------------------------- |
///   | `"sobel"` (def.) | 3×3 Sobel operator                      |
///   | `"prewitt"`      | 3×3 Prewitt                             |
///   | `"central"`      | central difference: `[1 0 -1] / 2`      |
///   | `"intermediate"` | forward difference: `[1 -1]`            |
///
/// Outputs are DOUBLE images of the same H×W as `I`.
///
/// @param I       Input image (any numeric class; cast to double).
/// @param method  Gradient kernel name (case-sensitive).
/// @param mr      Memory resource (nullptr → process default).
/// @return        `(Gx, Gy)`.
///
/// @see imgradient, edge
std::tuple<Value, Value>
imgradientxy(const Value &I, const std::string &method,
             std::pmr::memory_resource *mr = nullptr);

/// Gradient magnitude and direction (`[Gmag, Gdir] = imgradient(I, method)`).
///
/// Equivalent to running @ref imgradientxy and converting to polar:
///   @f$ G_\text{mag} = \sqrt{G_x^2 + G_y^2} @f$,
///   @f$ G_\text{dir} = \text{atan2}(-G_y, G_x) @f$ in degrees
///   (−180 ≤ Gdir ≤ 180; the `-Gy` flip makes the
///   angle increase counter-clockwise in screen coordinates).
///
/// @see imgradientxy, edge
std::tuple<Value, Value>
imgradient(const Value &I, const std::string &method,
           std::pmr::memory_resource *mr = nullptr);

/// 3-D component gradients
/// (`[Gx, Gy, Gz] = imgradientxyz(V, method)`).
///
/// Supports `"sobel"` (default), `"prewitt"`, `"central"`, and
/// `"intermediate"`. The Sobel / Prewitt kernels are the standard
/// 3×3×3 separable extensions used by MATLAB R2025b (Sobel weights
/// `[1, 3, 3, 1]`-style — not the naive `[1, 2, 1]` 2-D extension).
/// `"central"` ≡ `gradient(V)`; `"intermediate"` ≡ forward `diff`
/// with the trailing slice zero-padded.
///
/// Replicate boundary handling for the convolution kernels.
/// Output class is `single` if `V` is `single`, else `double`.
///
/// @param V       3-D grayscale volume.
/// @param method  `"sobel"` / `"prewitt"` / `"central"` /
///                `"intermediate"`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(Gx, Gy, Gz)` (same shape as `V`).
std::tuple<Value, Value, Value>
imgradientxyz(const Value &V, const std::string &method,
              std::pmr::memory_resource *mr = nullptr);

/// 3-D gradient magnitude + spherical direction
/// (`[Gmag, Gaz, Gelev] = imgradient3(V, method)` or
/// `(Gx, Gy, Gz)` inputs).
///
/// `Gmag = hypot(hypot(Gx, Gy), Gz)`,
/// `Gaz = atan2(-Gy, Gx)` (degrees),
/// `Gelev = atan2(Gz, hypot(Gx, Gy))` (degrees).
///
/// Single-output form returns only `Gmag`. The `(Gx, Gy, Gz)`
/// alternate form skips the convolution step (used when the
/// caller already has the directional gradients).
///
/// @see imgradientxyz
std::tuple<Value, Value, Value>
imgradient3(const Value &V, const std::string &method,
            std::pmr::memory_resource *mr = nullptr);
std::tuple<Value, Value, Value>
imgradient3_from_grads(const Value &Gx, const Value &Gy, const Value &Gz,
                       std::pmr::memory_resource *mr = nullptr);

/// Binary edge map (`BW = edge(I, method, thresh_lo, thresh_hi)`).
///
/// Detector dispatch:
///
///   | method        | description                                    |
///   | ------------- | ---------------------------------------------- |
///   | `"sobel"` (d) | gradient magnitude > thresh                    |
///   | `"prewitt"`   | gradient magnitude > thresh                    |
///   | `"roberts"`   | 2×2 cross-difference                           |
///   | `"canny"`     | Canny edge detector (uses both thresholds)     |
///   | `"log"`       | Laplacian of Gaussian, zero crossings ± thresh |
///   | `"zerocross"` | zero crossings of a user kernel (not yet wired)|
///
/// `thresh_lo` / `thresh_hi` accept `NaN` to request the auto-pick
/// heuristic (30 % of max gradient for the simple detectors; Otsu /
/// fraction-based for Canny).
///
/// @param I          Input image.
/// @param method     Detector name.
/// @param thresh_lo  Lower threshold or NaN.
/// @param thresh_hi  Upper threshold (Canny) or NaN.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Logical edge map.
///
/// @see imgradient
Value edge(const Value &I, const std::string &method,
           double thresh_lo, double thresh_hi,
           std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
