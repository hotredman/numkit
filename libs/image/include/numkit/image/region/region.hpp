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

/// bwlabel(BW[, conn]) — label connected components of a binary image.
/// `conn` ∈ {4, 8} (default 8). Returns (L, num) where L is uint16
/// label image and num is the count of components.
std::tuple<Value, Value>
bwlabel(const Value &BW, int conn, std::pmr::memory_resource *mr = nullptr);

/// bwconncomp(BW[, conn]) — connected-component labeling that returns
/// a MATLAB-style 1×1 struct with fields:
///   Connectivity (scalar), ImageSize ([H W]), NumObjects (scalar),
///   PixelIdxList (1×K cell of column-vector 1-based linear indices).
Value bwconncomp(const Value &BW, int conn, std::pmr::memory_resource *mr = nullptr);

/// bwarea(BW) — total area (number of foreground pixels with optional
/// quarter-pixel boundary correction). For first cut, returns the
/// integer pixel count.
Value bwarea(const Value &BW, std::pmr::memory_resource *mr = nullptr);

/// bwperim(BW[, conn]) — perimeter mask. Foreground pixel survives iff
/// at least one of its `conn`-neighbours is background.
Value bwperim(const Value &BW, int conn, std::pmr::memory_resource *mr = nullptr);

/// bwareaopen(BW, P[, conn]) — remove components with fewer than P
/// pixels.
Value bwareaopen(const Value &BW, int P, int conn, std::pmr::memory_resource *mr = nullptr);

/// bwboundaries(BW [, conn]) — trace outer boundaries of every
/// connected component via Moore-neighbour walking with Jacob's
/// stopping criterion. Returns a cell column where each entry is a
/// P×2 [row col] uint32 matrix listing boundary pixels in clockwise
/// order, including the start pixel as the closing entry.
Value bwboundaries(const Value &BW, int conn, std::pmr::memory_resource *mr = nullptr);

/// regionprops(BW_or_L [, props…]) — struct array of per-region
/// descriptors. Supported `props` (case-insensitive):
///   'Area'        : pixel count
///   'Centroid'    : 1×2 row [x y] in image coords (1-based)
///   'BoundingBox' : 1×4 row [xmin ymin width height]
///   'all'         : all of the above
/// Default (no `props`): all of the above.
/// Accepts either a binary image (runs bwlabel internally) or a
/// pre-labelled integer array.
Value regionprops(const Value &BW_or_L, const std::vector<std::string> &props, std::pmr::memory_resource *mr = nullptr);

/// `D = bwdist(BW)` — Euclidean distance transform. For each pixel
/// returns the distance to the nearest non-zero pixel in `BW`.
/// Foreground pixels themselves get distance 0; an all-zero input
/// yields +Inf everywhere. Implementation is the Felzenszwalb-
/// Huttenlocher 1-D parabolic-envelope DT applied row-wise then
/// column-wise (exact, O(H·W)).
Value bwdist(const Value &BW, std::pmr::memory_resource *mr = nullptr);

/// `BW = roicolor(A, low, high)` — region-of-interest mask via
/// inclusive range threshold: BW = (A >= low) & (A <= high).
/// `BW = roicolor(A, v)` — set-membership mask: BW(i) is true iff
/// A(i) matches any element of vector v. Output is logical, same
/// shape as A. Use range-form when `is_range` is true; otherwise
/// `low_or_v` is treated as a value vector.
Value roicolor(const Value &A, const Value &low_or_v, double high, bool is_range, std::pmr::memory_resource *mr = nullptr);

/// `fcc = fchcode(bound)` — Freeman 8-direction chain code for a
/// closed K-by-2 boundary (rows / cols). Returns a struct with
///   x0y0 — 1×2 start point (matching bound(1,:))
///   fcc  — 1×K direction codes
///   diff — 1×K mod-8 first-difference (cyclic)
/// Direction map: 3 2 1 / 4 . 0 / 5 6 7. If the boundary doesn't
/// already close on itself, the first point is appended.
Value fchcode(const Value &bound, std::pmr::memory_resource *mr = nullptr);

/// bweuler(BW [, n]) — Euler number (objects − holes) of a 2-D
/// binary image, computed via Pratt's bit-quad LUT method. `n`
/// is the connectivity for foreground (4 or 8); default 8.
Value bweuler(const Value &BW, int conn, std::pmr::memory_resource *mr = nullptr);

/// bwareafilt(BW, range_or_n [, keep_str] [, conn]) — keep
/// connected components by area.
///   - `range_or_n` is either a 2-element [lo hi] (inclusive
///     interval) or a scalar N (top-N selection).
///   - `keep_largest`: when N-form, true → "largest" (default),
///     false → "smallest".
///   - `conn` ∈ {4, 8}; default 8.
/// Output is a logical mask, same H × W as BW.
Value bwareafilt(const Value &BW, double lo, double hi, size_t n_keep, bool keep_largest, int conn, std::pmr::memory_resource *mr = nullptr);

/// `[imout, idx] = bwselect(BW, cols, rows[, conn])` — select all
/// connected components that contain any of the seed pixels
/// (`cols(k)`, `rows(k)`), 1-based. Connectivity 4 or 8 (default 8).
/// Returns a logical mask of the same size as BW; second output is a
/// column of 1-based linear (column-major) pixel indices that survive.
std::tuple<Value, Value>
bwselect(const Value &BW, const Value &cols, const Value &rows, int conn, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
