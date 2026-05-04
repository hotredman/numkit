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
bwlabel(std::pmr::memory_resource *mr, const Value &BW, int conn);

/// bwconncomp(BW[, conn]) — like bwlabel but returns a struct-like
/// 4-tuple (Connectivity, ImageSize, NumObjects, PixelIdxList). Since
/// numkit avoids OOП, we expose this as a 4-output function and
/// callers can destructure.
std::tuple<Value, Value, Value, Value>
bwconncomp(std::pmr::memory_resource *mr, const Value &BW, int conn);

/// bwarea(BW) — total area (number of foreground pixels with optional
/// quarter-pixel boundary correction). For first cut, returns the
/// integer pixel count.
Value bwarea(std::pmr::memory_resource *mr, const Value &BW);

/// bwperim(BW[, conn]) — perimeter mask. Foreground pixel survives iff
/// at least one of its `conn`-neighbours is background.
Value bwperim(std::pmr::memory_resource *mr, const Value &BW, int conn);

/// bwareaopen(BW, P[, conn]) — remove components with fewer than P
/// pixels.
Value bwareaopen(std::pmr::memory_resource *mr, const Value &BW, int P, int conn);

/// bwboundaries(BW [, conn]) — trace outer boundaries of every
/// connected component via Moore-neighbour walking with Jacob's
/// stopping criterion. Returns a cell column where each entry is a
/// P×2 [row col] uint32 matrix listing boundary pixels in clockwise
/// order, including the start pixel as the closing entry.
Value bwboundaries(std::pmr::memory_resource *mr,
                   const Value &BW, int conn);

/// regionprops(BW_or_L [, props…]) — struct array of per-region
/// descriptors. Supported `props` (case-insensitive):
///   'Area'        : pixel count
///   'Centroid'    : 1×2 row [x y] in image coords (1-based)
///   'BoundingBox' : 1×4 row [xmin ymin width height]
///   'all'         : all of the above
/// Default (no `props`): all of the above.
/// Accepts either a binary image (runs bwlabel internally) or a
/// pre-labelled integer array.
Value regionprops(std::pmr::memory_resource *mr,
                  const Value &BW_or_L,
                  const std::vector<std::string> &props);

/// `D = bwdist(BW)` — Euclidean distance transform. For each pixel
/// returns the distance to the nearest non-zero pixel in `BW`.
/// Foreground pixels themselves get distance 0; an all-zero input
/// yields +Inf everywhere. Implementation is the Felzenszwalb-
/// Huttenlocher 1-D parabolic-envelope DT applied row-wise then
/// column-wise (exact, O(H·W)).
Value bwdist(std::pmr::memory_resource *mr, const Value &BW);

/// bweuler(BW [, n]) — Euler number (objects − holes) of a 2-D
/// binary image, computed via Pratt's bit-quad LUT method. `n`
/// is the connectivity for foreground (4 or 8); default 8.
Value bweuler(std::pmr::memory_resource *mr, const Value &BW, int conn);

/// bwareafilt(BW, range_or_n [, keep_str] [, conn]) — keep
/// connected components by area.
///   - `range_or_n` is either a 2-element [lo hi] (inclusive
///     interval) or a scalar N (top-N selection).
///   - `keep_largest`: when N-form, true → "largest" (default),
///     false → "smallest".
///   - `conn` ∈ {4, 8}; default 8.
/// Output is a logical mask, same H × W as BW.
Value bwareafilt(std::pmr::memory_resource *mr,
                 const Value &BW, double lo, double hi,
                 size_t n_keep, bool keep_largest, int conn);

} // namespace numkit::image
