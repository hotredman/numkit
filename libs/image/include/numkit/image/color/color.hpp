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

} // namespace numkit::image
