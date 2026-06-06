// libs/image/include/numkit/image/type_convert/type_convert.hpp
//
// Image type conversion and class-related helpers.

#pragma once

#include <limits>
#include <memory_resource>
#include <string>
#include <tuple>
#include <numkit/value/value.hpp>

namespace numkit::image {

/// @file
/// @brief Image type-conversion helpers.
///
/// **Scaling conventions:**
/// - Float input is assumed to be in `[0, 1]` (clipped on conversion to
///   integer).
/// - Integer ↔ float conversions scale by class range:
///   * `uint8  ↔ [0, 1]`: ÷255 / ×255
///   * `uint16 ↔ [0, 1]`: ÷65535 / ×65535
///   * `int16  ↔ [0, 1]`: `(x + 32768) / 65535` / `round(x · 65535) - 32768`
/// - Integer-to-integer conversions use bit-replication for upscaling
///   (uint8 → uint16: `0xAB → 0xABAB`) and rounded scaling for downscaling.

/// @brief Convert to DOUBLE in `[0, 1]` (`B = im2double(A)`).
///
/// Integer inputs are scaled by their class range. Float inputs are
/// cast to DOUBLE without rescaling.
///
/// @param x   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE image in `[0, 1]`.
/// @see im2single, im2uint8, im2uint16, im2int16, mat2gray
Value im2double(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert to SINGLE in `[0, 1]` (`B = im2single(A)`).
///
/// Same scaling rules as @ref im2double.
///
/// @param x   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    SINGLE image in `[0, 1]`.
/// @see im2double
Value im2single(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert to UINT8 (`B = im2uint8(A)`).
///
/// See file note for scaling rules.
///
/// @param x   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    UINT8 image.
/// @see im2uint16, im2double
Value im2uint8(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert to UINT16 (`B = im2uint16(A)`).
///
/// @param x   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    UINT16 image.
/// @see im2uint8, im2double
Value im2uint16(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Convert to INT16 (`B = im2int16(A)`).
///
/// @param x   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    INT16 image.
/// @see im2uint16, im2double
Value im2int16(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Linear rescale to `[0, 1]` DOUBLE (`B = mat2gray(A, lo, hi)`).
///
/// Maps `[lo, hi]` linearly to `[0, 1]` and clips. If `lo` / `hi` are
/// NaN, they default to `min(A)` / `max(A)`. Useful for visualising
/// floating-point matrices.
///
/// @param x   Input matrix.
/// @param lo  Low intensity (NaN → auto = min(x)).
/// @param hi  High intensity (NaN → auto = max(x)).
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE image in `[0, 1]`.
/// @see im2double
Value mat2gray(const Value &x,
               double lo = std::numeric_limits<double>::quiet_NaN(),
               double hi = std::numeric_limits<double>::quiet_NaN(),
               std::pmr::memory_resource *mr = nullptr);

/// @brief RGB → grayscale dispatcher (`Y = im2gray(X)`).
///
/// If `X` has 3 pages, applies @ref rgb2gray; otherwise returns `X`
/// unchanged (already grayscale).
///
/// @param x   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Grayscale image.
/// @see rgb2gray
Value im2gray(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Rec. 601 luminance (`Y = rgb2gray(X)`).
///
/// @f$ Y = 0.2989\,R + 0.5870\,G + 0.1140\,B @f$. Output class matches
/// input.
///
/// @param x   Input RGB image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Grayscale image, same class as `x`.
/// @see im2gray
Value rgb2gray(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Class display range (`r = getrangefromclass(I)`).
///
/// Returns the conventional `[min, max]` for the class of `I`:
/// - integer types: `intmin` / `intmax` of the class.
/// - LOGICAL, SINGLE, DOUBLE: `[0, 1]`.
///
/// @param I   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `1 × 2` DOUBLE row.
Value getrangefromclass(const Value &I, std::pmr::memory_resource *mr = nullptr);

/// @brief Grayscale → indexed (`[ind, map] = gray2ind(I, n)`).
///
/// Quantises `I` into `n` bins. Float input must be in `[0, 1]`;
/// integer input is rescaled. LOGICAL default `n = 2`; otherwise
/// `n = 64`. Output `ind` is UINT8 if `n <= 256`, else UINT16.
/// `map` is the linear `gray(n)` colormap (`n × 3`).
///
/// @param I   Input grayscale image.
/// @param n   Bin count.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(ind, map)` pair.
/// @see ind2gray, ind2rgb
std::tuple<Value, Value>
gray2ind(const Value &I, int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Indexed → grayscale (`I = ind2gray(idx, map)`).
///
/// Pulls the luminance value out of `map` for each entry in `idx`.
/// Pass `Value::Empty` for `map` to use `gray(N)` where `N = max(idx)`.
///
/// @param idx  Index image.
/// @param map  Colormap.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Grayscale image.
/// @see gray2ind
Value ind2gray(const Value &idx, const Value &map,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Indexed → RGB (`RGB = ind2rgb(idx, map)`).
///
/// Float `idx` is 1-based; integer `idx` is 0-based.
/// Out-of-range entries clip to the first / last colormap row.
///
/// @param idx  Index image.
/// @param map  Colormap.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `M × N × 3` DOUBLE image.
/// @see gray2ind, ind2gray
Value ind2rgb(const Value &idx, const Value &map,
              std::pmr::memory_resource *mr = nullptr);

/// @brief English ordinal-form string (`s = iptnum2ordinal(n)`).
///
/// `1..20` return English words (`"first"` … `"twentieth"`); `21+`
/// use suffix form (`"21st"`, `"22nd"`, `"23rd"`, `"24th"`, …).
///
/// @param n   Positive integer (non-integer rejected).
/// @param mr  Memory resource (nullptr → process default).
/// @return    CHAR Value.
/// @throws Error  Non-integer or non-positive `n`.
Value iptnum2ordinal(double n, std::pmr::memory_resource *mr = nullptr);

/// @brief Generic class converter (`B = imcast(I, type)`).
///
/// Dispatches to the appropriate `im2*` helper based on the target
/// class name. LOGICAL target maps non-zero entries to 1; casting
/// from LOGICAL to integer maps true → `intmax` of the target.
/// The `"indexed"` third-arg mode is not yet supported.
///
/// @param I     Input image.
/// @param type  Target class: `"double"`, `"single"`, `"uint8"`,
///              `"uint16"`, `"int16"`, `"logical"`.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Image cast to `type`.
/// @throws Error  Unknown / unsupported type name.
Value imcast(const Value &I, const std::string &type,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Binary-image predicate (`tf = isbw(BW, mode)`).
///
/// - `mode = "logical"` (default): requires class LOGICAL.
/// - `mode = "non-logical"`: accepts numeric classes whose values are
///   all 0 or 1 (no NaN).
///
/// Spatial check: 2-D or `M × N × 1 × K` with `pages == 1`.
///
/// @param BW    Input image.
/// @param mode  `"logical"` or `"non-logical"`.
/// @param mr    Memory resource (nullptr → process default).
/// @return      LOGICAL scalar.
/// @see isgray, isind, isrgb
Value isbw(const Value &BW, const std::string &mode,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Grayscale-image predicate (`tf = isgray(I)`).
///
/// True if `I` is plausibly grayscale: 2-D or `M × N × 1 × K`, and
/// either an integer-class image (UINT8 / UINT16 / INT16) or a float
/// image whose values lie in `[0, 1]` or are `NaN` (not all NaN).
///
/// @param I   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL scalar.
/// @see isbw, isrgb
Value isgray(const Value &I, std::pmr::memory_resource *mr = nullptr);

/// @brief Indexed-image predicate (`tf = isind(I)`).
///
/// True if `I` is plausibly indexed: 2-D or `M × N × 1 × K`, and either
/// UINT8 / UINT16 or a float image whose values are positive integers.
///
/// @param I   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL scalar.
/// @see isgray, gray2ind
Value isind(const Value &I, std::pmr::memory_resource *mr = nullptr);

/// @brief RGB-image predicate (`tf = isrgb(I)`).
///
/// True if `I` is plausibly RGB: `M × N × 3` (or `M × N × 3 × K`),
/// and either an integer-class image or a float image with values in
/// `[0, 1]` (not all NaN).
///
/// @param I   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL scalar.
/// @see isgray
Value isrgb(const Value &I, std::pmr::memory_resource *mr = nullptr);

/// @brief Colormap-shape predicate (`tf = iscolormap(cmap)`).
///
/// True iff `cmap` is a real, float (SINGLE / DOUBLE) 2-D matrix with
/// exactly 3 columns and `>= 1` row. The `[0, 1]` range is **not**
/// enforced (matches Octave behaviour).
///
/// @param cmap  Candidate colormap.
/// @param mr    Memory resource (nullptr → process default).
/// @return      LOGICAL scalar.
Value iscolormap(const Value &cmap, std::pmr::memory_resource *mr = nullptr);

/// @brief Apply a look-up table to an integer image
/// (`B = intlut(A, LUT)`).
///
/// `A` must be UINT8, UINT16, or INT16. `LUT` must have:
/// - 256 elements for UINT8 input.
/// - 65536 elements for UINT16 / INT16 input.
///
/// For INT16, the index is `A(i) + 32768`.
/// Output class equals `class(LUT)`.
///
/// @param A    Integer image.
/// @param LUT  Lookup table vector.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Image with LUT applied.
/// @throws Error  Wrong LUT length or non-integer input.
Value intlut(const Value &A, const Value &LUT,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
