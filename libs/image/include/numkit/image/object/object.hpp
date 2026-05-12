// libs/image/include/numkit/image/object/object.hpp
//
// Object analysis: gradient / edge detection.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::image {

/// imgradientxy(I[, method]) — returns (Gx, Gy) component gradients.
/// method: "sobel" (default), "prewitt", "central", "intermediate".
std::tuple<Value, Value>
imgradientxy(const Value &I, const std::string &method, std::pmr::memory_resource *mr = nullptr);

/// imgradient(I[, method]) — magnitude / direction of gradient.
/// Returns (Gmag, Gdir) where Gdir is in degrees.
std::tuple<Value, Value>
imgradient(const Value &I, const std::string &method, std::pmr::memory_resource *mr = nullptr);

/// edge(I[, method, thresh]) — binary edge map.
/// method: "sobel" (default), "prewitt", "roberts", "canny", "log",
///         "zerocross".
/// thresh: scalar threshold (NaN = auto-pick at 30% of max gradient).
/// For Canny, thresh is a 2-vector [low, high]; auto-pick uses
/// Otsu / fraction heuristics if NaN.
Value edge(const Value &I, const std::string &method, double thresh_lo, double thresh_hi, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
