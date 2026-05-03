// libs/image/include/numkit/image/filter/filter.hpp
//
// Image filtering primitives. Portable scalar; SIMD planned for a
// later phase.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <vector>

namespace numkit::image {

enum class PadMode {
    Constant,    // pad with `pad_value`
    Replicate,   // repeat edge pixel
    Symmetric,   // mirror without duplicating edge: a b c | b a
    Circular     // wrap-around
};

/// padarray(A, padsize[, mode|val][, direction]) — pad A by `padsize`
/// elements on each side. `padsize` is a 1-D vector with one entry per
/// dimension. Direction: "both" (default), "pre", "post".
Value padarray(std::pmr::memory_resource *mr, const Value &x,
               const std::vector<int> &padsize,
               PadMode mode, double pad_value,
               const std::string &direction);

/// fspecial(type, ...) — returns a 2-D filter kernel as a DOUBLE matrix.
/// Supported types: 'average', 'gaussian', 'laplacian', 'log',
/// 'sobel', 'prewitt', 'disk'.
Value fspecial(std::pmr::memory_resource *mr,
               const std::string &type,
               const std::vector<double> &params);

/// imfilter(I, h, [boundary, output_size, conv_or_corr])
///   boundary: 0 (default), 'replicate', 'symmetric', 'circular'
///   output_size: 'same' (default) or 'full'
///   conv_or_corr: 'corr' (default) or 'conv'
Value imfilter(std::pmr::memory_resource *mr,
               const Value &I, const Value &h,
               PadMode boundary, double pad_value,
               bool full, bool flip_kernel);

/// imgaussfilt(I, sigma[, FilterSize]) — 2-D Gaussian filtering with
/// boundary='replicate', output='same'.
Value imgaussfilt(std::pmr::memory_resource *mr,
                  const Value &I, double sigma, int filter_size);

/// imboxfilt(I, FilterSize) — local mean filter with replicate boundary.
Value imboxfilt(std::pmr::memory_resource *mr, const Value &I, int filter_size);

/// medfilt2(I[, [m n]]) — 2-D median filter. Default 3×3.
Value medfilt2(std::pmr::memory_resource *mr, const Value &I,
               int rows = 3, int cols = 3);

} // namespace numkit::image
