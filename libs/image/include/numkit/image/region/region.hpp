// libs/image/include/numkit/image/region/region.hpp
//
// Connected-component labelling and basic region descriptors.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>
#include <vector>

namespace numkit::image {

/// Connected-component labelling (`[L, num] = bwlabel(BW, conn)`).
///
/// Two-pass union-find labelling. Returns the label image as uint16
/// (0 = background) and the component count.
///
/// @param BW    Binary image.
/// @param conn  Connectivity — 4 or 8 (default 8).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `(L, num)`; bind via `auto [L, num] = bwlabel(BW, 8);`.
///
/// @see bwconncomp, bwareaopen, bwselect
std::tuple<Value, Value>
bwlabel(const Value &BW, int conn,
        std::pmr::memory_resource *mr = nullptr);

/// @brief Connected-component struct
/// (`CC = bwconncomp(BW, conn)`).
///
/// Returns a 1×1 struct with fields:
/// - `Connectivity`  — scalar (`conn`).
/// - `ImageSize`     — `[H W]`.
/// - `NumObjects`    — scalar count.
/// - `PixelIdxList`  — 1×K cell of column-vectors of 1-based linear
///                     (column-major) indices per component.
///
/// @param BW    Binary image.
/// @param conn  Connectivity — 4 or 8 (default 8).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Struct Value with the fields described above.
/// @see bwlabel
Value bwconncomp(const Value &BW, int conn,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Total foreground area (`A = bwarea(BW)`).
///
/// Returns the integer pixel count (the optional quarter-pixel
/// boundary correction is not applied).
///
/// @param BW  Binary image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar area in pixels.
/// @see bwperim
Value bwarea(const Value &BW,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Perimeter mask (`BW2 = bwperim(BW, conn)`).
///
/// A foreground pixel survives iff at least one of its
/// `conn`-neighbours is background.
///
/// @param BW    Binary image.
/// @param conn  Connectivity (4 or 8).
/// @param mr    Memory resource (nullptr → process default).
/// @return      LOGICAL perimeter mask, same shape as `BW`.
/// @see bwarea, bwboundaries
Value bwperim(const Value &BW, int conn,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Remove small components (`BW2 = bwareaopen(BW, P, conn)`).
///
/// Drops every connected component with fewer than `P` foreground
/// pixels.
///
/// @param BW    Binary image.
/// @param P     Minimum component size (pixels).
/// @param conn  Connectivity (4 or 8).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Filtered LOGICAL mask, same shape as `BW`.
/// @see bwareafilt
Value bwareaopen(const Value &BW, int P, int conn,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Trace boundary contours (`B = bwboundaries(BW, conn)`).
///
/// Moore-neighbour walking with Jacob's stopping criterion. Returns
/// a cell column where each entry is a P×2 uint32 matrix listing
/// `[row col]` boundary pixels in clockwise order, with the start
/// pixel appended as the closing entry.
///
/// @param BW    Binary image.
/// @param conn  Connectivity (4 or 8).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Cell column of P×2 coordinate arrays.
/// @see bwperim, bwconncomp
Value bwboundaries(const Value &BW, int conn,
                   std::pmr::memory_resource *mr = nullptr);

/// Per-region descriptors (`S = regionprops(BW_or_L, props)`).
///
/// Accepts either a binary image (`bwlabel` is run internally) or a
/// pre-labelled integer image.
///
/// Supported props (case-insensitive):
///   - `"Area"`         : pixel count.
///   - `"Centroid"`     : 1×2 row `[x y]` in image coords (1-based).
///   - `"BoundingBox"`  : 1×4 row `[xmin ymin width height]`.
///   - `"all"`          : all of the above.
///
/// Default (empty `props`): all of the above.
///
/// @param BW_or_L  Binary or labelled image.
/// @param props    Property names to compute.
/// @param mr       Memory resource (nullptr → process default).
/// @return         Struct array (one element per region).
Value regionprops(const Value &BW_or_L,
                  const std::vector<std::string> &props,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Euclidean distance transform (`D = bwdist(BW)`).
///
/// For each pixel, the distance to the nearest non-zero pixel in
/// `BW`. Foreground pixels themselves get distance 0; an all-zero
/// input yields `+Inf` everywhere. Implementation is the
/// Felzenszwalb–Huttenlocher 1-D parabolic-envelope DT applied
/// row-wise then column-wise — exact, O(H·W).
///
/// @param BW  Binary image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    DOUBLE distance map, same shape as `BW`.
Value bwdist(const Value &BW,
             std::pmr::memory_resource *mr = nullptr);

/// Region-of-interest mask (`BW = roicolor(A, low, high)`).
///
/// Two forms (selected by `is_range`):
///   - Range form: `BW = (A >= low) & (A <= high)` — inclusive.
///   - Set form (`is_range = false`): `BW(i) = any(A(i) == v)` where
///     `v = low_or_v` is treated as a vector.
///
/// @param A          Input image.
/// @param low_or_v   Range low / value vector.
/// @param high       Range high (ignored in set form).
/// @param is_range   `true` → range form, `false` → set form.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Logical mask same shape as `A`.
Value roicolor(const Value &A, const Value &low_or_v, double high,
               bool is_range,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Freeman 8-direction chain code (`fcc = fchcode(bound)`).
///
/// Encodes a closed K×2 boundary (rows / cols) as a sequence of
/// 8-direction codes. Direction map:
///
///     3 2 1
///     4 . 0
///     5 6 7
///
/// Returns a struct with fields:
/// - `x0y0` — 1×2 start point (matches `bound(1, :)`).
/// - `fcc`  — 1×K direction codes.
/// - `diff` — 1×K mod-8 first-difference (cyclic).
///
/// If the boundary doesn't already close on itself, the first point
/// is appended automatically.
///
/// @param bound  Boundary K×2 matrix (rows, cols).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Struct with `x0y0`, `fcc`, `diff` fields.
/// @see bwboundaries
Value fchcode(const Value &bound,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Euler number `objects − holes`
/// (`E = bweuler(BW, conn)`).
///
/// Computed via Pratt's bit-quad LUT method.
///
/// @param BW    Binary image.
/// @param conn  Foreground connectivity — 4 or 8 (default 8).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Scalar Euler number.
Value bweuler(const Value &BW, int conn,
              std::pmr::memory_resource *mr = nullptr);

/// Filter connected components by area (`BW2 = bwareafilt(...)`).
///
/// Two selection modes:
///   - **Range mode** (`lo` and `hi` finite): keep components whose
///     area lies in `[lo, hi]`.
///   - **Top-N mode** (`n_keep > 0`): keep the `n_keep` largest
///     (`keep_largest = true`) or smallest (`false`) components.
///
/// @param BW            Binary input.
/// @param lo            Range lower bound (range mode).
/// @param hi            Range upper bound (range mode).
/// @param n_keep        Component count (top-N mode).
/// @param keep_largest  Tie-break direction for top-N.
/// @param conn          Connectivity (4 or 8; default 8).
/// @param mr            Memory resource (nullptr → process default).
/// @return              Logical mask, same H×W as `BW`.
Value bwareafilt(const Value &BW, double lo, double hi,
                 size_t n_keep, bool keep_largest, int conn,
                 std::pmr::memory_resource *mr = nullptr);

/// Select components by seed pixels
/// (`[BW2, idx] = bwselect(BW, cols, rows, conn)`).
///
/// Returns the union of every connected component that contains at
/// least one of the seed pixels `(cols(k), rows(k))` (1-based).
///
/// @param BW    Binary image.
/// @param cols  Seed column coordinates (1-based).
/// @param rows  Seed row coordinates (1-based).
/// @param conn  Connectivity (4 or 8).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `(BW2, idx)` — selected mask + column of surviving
///              1-based linear pixel indices.
std::tuple<Value, Value>
bwselect(const Value &BW, const Value &cols, const Value &rows, int conn,
         std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
