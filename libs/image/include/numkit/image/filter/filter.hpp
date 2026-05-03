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

} // namespace numkit::image
