// libs/image/include/numkit/image/type_convert/type_convert.hpp
//
// Image type conversion. MATLAB-style conventions:
//   - Float input is assumed to be in [0, 1] (clipped on conversion to int).
//   - Integer ↔ float conversions scale by class range:
//       uint8  ↔ [0, 1]   :  ÷255 / ×255
//       uint16 ↔ [0, 1]   :  ÷65535 / ×65535
//       int16  ↔ [0, 1]   :  (x + 32768) / 65535  /  round(x*65535) - 32768
//   - Integer-to-integer conversions use bit-replication for upscaling
//     (uint8 → uint16: 0xAB → 0xABAB) and rounded scaling for downscaling.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::image {

Value im2double(std::pmr::memory_resource *mr, const Value &x);
Value im2single(std::pmr::memory_resource *mr, const Value &x);
Value im2uint8 (std::pmr::memory_resource *mr, const Value &x);
Value im2uint16(std::pmr::memory_resource *mr, const Value &x);
Value im2int16 (std::pmr::memory_resource *mr, const Value &x);

/// mat2gray(A) or mat2gray(A, [lo, hi]) — rescale to [0, 1] double.
Value mat2gray(std::pmr::memory_resource *mr, const Value &x,
               double lo = std::numeric_limits<double>::quiet_NaN(),
               double hi = std::numeric_limits<double>::quiet_NaN());

/// im2gray(X) — RGB → grayscale if X has 3 pages; otherwise return X.
Value im2gray (std::pmr::memory_resource *mr, const Value &x);

/// rgb2gray(X) — Y = 0.2989·R + 0.5870·G + 0.1140·B (Rec. 601).
/// Output class matches input.
Value rgb2gray(std::pmr::memory_resource *mr, const Value &x);

/// `getrangefromclass(I)` — return the display [min, max] for the
/// class of `I`. Integer types use intmin/intmax; logical, single,
/// and double all return [0, 1]. Output is always double.
Value getrangefromclass(std::pmr::memory_resource *mr, const Value &I);

/// `[ind, map] = gray2ind(I [, n])` — grayscale → indexed image.
/// Float input must be in [0, 1]; integer input is rescaled. Logical
/// default n=2 else n=64. Output is uint8 if n ≤ 256, else uint16.
/// `map` is the linear gray(n) N×3 colormap.
std::tuple<Value, Value>
gray2ind(std::pmr::memory_resource *mr, const Value &I, int n);

/// `I = ind2gray(idx [, map])` — pull the luminance value out of
/// the colormap `map` for each entry in `idx`. Default `map` is
/// gray(N) where N = max(idx).
Value ind2gray(std::pmr::memory_resource *mr,
               const Value &idx, const Value &map);

/// `RGB = ind2rgb(idx, map)` — convert indexed image to RGB. Float
/// idx is 1-based; integer idx is 0-based. Out-of-range entries
/// clip to first / last colormap row. Output is M×N×3 double.
Value ind2rgb(std::pmr::memory_resource *mr,
              const Value &idx, const Value &map);

/// `iptnum2ordinal(n)` — return an ordinal-form string for the
/// positive integer `n`. 1..20 use English words ("first" …
/// "twentieth"); 21+ use the suffix form ("21st", "22nd", "23rd",
/// "24th", …).
Value iptnum2ordinal(std::pmr::memory_resource *mr, double n);

/// `imcast(I, type)` — convert image to a target class string
/// ("double", "single", "uint8", "uint16", "int16", "logical").
/// Dispatches to the existing im2double/im2single/im2uint8/...
/// helpers; logical-target maps non-zero entries to 1, and casting
/// from logical to integer maps true → intmax of the target class.
/// The MATLAB "indexed" third-arg mode is not yet supported.
Value imcast(std::pmr::memory_resource *mr,
             const Value &I, const std::string &type);

/// `isbw(BW [, mode])` — true if `BW` looks like a binary image.
/// `mode = "logical"` (default) requires class logical; `mode =
/// "non-logical"` accepts numeric classes whose values are all 0 or
/// 1 (no NaN). Spatial check: 2-D, or M×N×1×K with pages == 1.
Value isbw   (std::pmr::memory_resource *mr, const Value &BW,
              const std::string &mode);

/// `isgray(I)` — true if `I` is plausibly a grayscale image:
/// 2-D / M×N×1×K, and either an integer-class image (uint8 / uint16
/// / int16) or a floating-point image whose values are in [0, 1] or
/// NaN (not all NaN).
Value isgray (std::pmr::memory_resource *mr, const Value &I);

/// `isind(I)` — true if `I` is plausibly an indexed image:
/// 2-D / M×N×1×K, and either an integer-class image (uint8 / uint16)
/// or a floating-point image whose values are positive integers.
Value isind  (std::pmr::memory_resource *mr, const Value &I);

/// `isrgb(I)` — true if `I` is plausibly an RGB image:
/// M×N×3 (or M×N×3×K), and either an integer-class image or a
/// float-class image with values in [0, 1] (not all NaN).
Value isrgb  (std::pmr::memory_resource *mr, const Value &I);

/// intlut(A, LUT) — apply a lookup table to an integer image.
/// `A` must be uint8, uint16, or int16. `LUT` must be a vector of
///   256        elements for uint8 input,
///   65536      elements for uint16 / int16 input.
/// For int16, the index is `A(i) + 32768` to match MATLAB semantics.
/// Output class equals `class(LUT)`.
Value intlut(std::pmr::memory_resource *mr, const Value &A, const Value &LUT);

} // namespace numkit::image
