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

#include <limits>
#include <memory_resource>
#include <string>
#include <tuple>
#include <numkit/core/value.hpp>

namespace numkit::image {

/// Convert image to double in [0, 1] (`B = im2double(A)`).
///
/// Integer inputs are scaled by their class range (uint8 ÷ 255,
/// uint16 ÷ 65535, int16 → `(x + 32768)/65535`). Float inputs are
/// returned as-is (cast to double).
///
/// @see im2single, im2uint8, im2uint16, im2int16, mat2gray
Value im2double(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Convert image to single (float32) in [0, 1] — see @ref im2double.
Value im2single(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Convert image to uint8 — see @ref im2double for the scaling rules.
Value im2uint8 (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Convert image to uint16 — see @ref im2double for the scaling rules.
Value im2uint16(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Convert image to int16 — see @ref im2double for the scaling rules.
Value im2int16 (const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Linear rescale to [0, 1] double (`B = mat2gray(A, [lo, hi])`).
///
/// Maps `[lo, hi]` linearly to `[0, 1]` and clips. If `lo` / `hi` are
/// NaN, they default to `min(A)` / `max(A)`. Useful for visualising
/// floating-point matrices.
///
/// @param x   Input matrix.
/// @param lo  Low intensity (NaN → auto = min(x)).
/// @param hi  High intensity (NaN → auto = max(x)).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Double image in [0, 1].
Value mat2gray(const Value &x,
               double lo = std::numeric_limits<double>::quiet_NaN(),
               double hi = std::numeric_limits<double>::quiet_NaN(),
               std::pmr::memory_resource *mr = nullptr);

/// Convert RGB → grayscale (`Y = im2gray(X)`).
///
/// If `X` has 3 pages, applies @ref rgb2gray; otherwise returns `X`
/// unchanged (already grayscale).
Value im2gray(const Value &x,
              std::pmr::memory_resource *mr = nullptr);

/// Standard luminance grayscale (`Y = rgb2gray(X)`).
///
/// @f$ Y = 0.2989\,R + 0.5870\,G + 0.1140\,B @f$ (Rec. 601 luminance).
/// Output class matches input.
Value rgb2gray(const Value &x,
               std::pmr::memory_resource *mr = nullptr);

/// Class display range (`r = getrangefromclass(I)`).
///
/// Returns the conventional `[min, max]` for the class of `I`:
///   - integer types: `intmin`/`intmax` of the class.
///   - logical, single, double: `[0, 1]`.
///
/// Output is always double.
Value getrangefromclass(const Value &I,
                        std::pmr::memory_resource *mr = nullptr);

/// Grayscale → indexed image (`[ind, map] = gray2ind(I, n)`).
///
/// Quantises `I` into `n` bins. Float input must be in [0, 1];
/// integer input is rescaled. Logical default `n = 2`; otherwise
/// `n = 64`. Output `ind` is uint8 if `n ≤ 256`, otherwise uint16.
/// `map` is the linear `gray(n)` colormap (n × 3).
std::tuple<Value, Value>
gray2ind(const Value &I, int n,
         std::pmr::memory_resource *mr = nullptr);

/// Indexed → grayscale (`I = ind2gray(idx, map)`).
///
/// Pulls the luminance value out of `map` for each entry in `idx`.
/// Default `map` is `gray(N)` where `N = max(idx)`.
Value ind2gray(const Value &idx, const Value &map,
               std::pmr::memory_resource *mr = nullptr);

/// Indexed → RGB (`RGB = ind2rgb(idx, map)`).
///
/// Float `idx` is 1-based; integer `idx` is 0-based (MATLAB
/// convention). Out-of-range entries clip to the first / last
/// colormap row. Output is M×N×3 double.
Value ind2rgb(const Value &idx, const Value &map,
              std::pmr::memory_resource *mr = nullptr);

/// English ordinal-form string (`s = iptnum2ordinal(n)`).
///
/// `1..20` return English words (`"first"` … `"twentieth"`); `21+`
/// use suffix form (`"21st"`, `"22nd"`, `"23rd"`, `"24th"`, …).
///
/// @param n   Positive integer (non-integer is rejected).
/// @param mr  Memory resource (nullptr → process default).
/// @return    String Value.
Value iptnum2ordinal(double n,
                     std::pmr::memory_resource *mr = nullptr);

/// Generic class converter (`B = imcast(I, type)`).
///
/// Dispatches to the appropriate `im2*` helper based on the target
/// class name (`"double"`, `"single"`, `"uint8"`, `"uint16"`, `"int16"`,
/// `"logical"`). Logical-target maps non-zero entries to 1; casting
/// from logical to integer maps true → intmax of the target.
///
/// MATLAB's `"indexed"` third-arg mode is not yet supported.
///
/// @param I     Input image.
/// @param type  Target class name (case-sensitive).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Image cast to `type`.
/// @throws      Error on unknown / unsupported type name.
Value imcast(const Value &I, const std::string &type,
             std::pmr::memory_resource *mr = nullptr);

/// Binary-image predicate (`tf = isbw(BW, mode)`).
///
/// - `mode = "logical"` (default): requires class logical.
/// - `mode = "non-logical"`: accepts numeric classes whose values
///   are all 0 or 1 (no NaN).
///
/// Spatial check: 2-D or M×N×1×K with `pages == 1`.
Value isbw(const Value &BW, const std::string &mode,
           std::pmr::memory_resource *mr = nullptr);

/// Grayscale-image predicate (`tf = isgray(I)`).
///
/// True if `I` is plausibly grayscale: 2-D or M×N×1×K, and either an
/// integer-class image (uint8 / uint16 / int16) or a float image
/// whose values lie in [0, 1] or are NaN (not all NaN).
Value isgray(const Value &I,
             std::pmr::memory_resource *mr = nullptr);

/// Indexed-image predicate (`tf = isind(I)`).
///
/// True if `I` is plausibly indexed: 2-D or M×N×1×K, and either
/// uint8 / uint16 or a float image whose values are positive
/// integers.
Value isind(const Value &I,
            std::pmr::memory_resource *mr = nullptr);

/// RGB-image predicate (`tf = isrgb(I)`).
///
/// True if `I` is plausibly RGB: M×N×3 (or M×N×3×K), and either an
/// integer-class image or a float image with values in [0, 1] (not
/// all NaN).
Value isrgb(const Value &I,
            std::pmr::memory_resource *mr = nullptr);

/// Colormap-shape predicate (`tf = iscolormap(cmap)`).
///
/// True iff `cmap` is a valid colormap: a real, float (single /
/// double) 2-D matrix with exactly 3 columns and ≥ 1 row. The [0, 1]
/// range is **not** enforced (Octave behaviour).
Value iscolormap(const Value &cmap,
                 std::pmr::memory_resource *mr = nullptr);

/// Apply a look-up table to an integer image (`B = intlut(A, LUT)`).
///
/// `A` must be uint8, uint16, or int16. `LUT` must have:
///   - 256 elements for uint8 input.
///   - 65536 elements for uint16 / int16 input.
///
/// For int16, the index is `A(i) + 32768` to match MATLAB semantics.
/// Output class equals `class(LUT)`.
///
/// @param A    Integer image.
/// @param LUT  Lookup table vector.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Image with LUT applied.
Value intlut(const Value &A, const Value &LUT,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
