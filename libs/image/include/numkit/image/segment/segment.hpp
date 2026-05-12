// libs/image/include/numkit/image/segment/segment.hpp
//
// Image-segmentation utilities and similarity metrics. The
// "deep-learning" entries from MATLAB's segment family
// (segmentAnythingModel, imsegsam, …) are intentionally omitted.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::image {

/// `d = dice(BW1, BW2)` — Sørensen-Dice similarity coefficient
///   d = 2·|A ∩ B| / (|A| + |B|)
/// Operates on binary masks (any non-zero pixel counts as foreground).
/// Returns 1 when both masks are empty (degenerate convention).
Value dice(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// `j = jaccard(BW1, BW2)` — intersection-over-union (IoU).
///   j = |A ∩ B| / |A ∪ B|
/// Same conventions as `dice`. Returns 1 when both masks are empty.
Value jaccard(const Value &A, const Value &B, std::pmr::memory_resource *mr = nullptr);

/// `BW = boundarymask(L_or_BW [, conn])` — logical mask of pixels
/// that share a connectivity-neighbour with a *different* label
/// (equivalently: a region edge in the label image).
/// `conn` ∈ {4, 8} (default 8). For binary inputs, returns the
/// foreground perimeter, similar to `bwperim`.
Value boundarymask(const Value &L_or_BW, int conn, std::pmr::memory_resource *mr = nullptr);

/// `idx = label2idx(L)` — column cell array; entry k is a column
/// of MATLAB-1-based linear indices for label k. Pixels labelled 0
/// are excluded.
Value label2idx(const Value &L, std::pmr::memory_resource *mr = nullptr);

/// `BW = grayconnected(I, row, col [, tol])` — flood-fill from seed
/// (row, col) (1-based MATLAB), accepting any 8-connected neighbour
/// whose intensity differs from the seed pixel's value by ≤ tol.
/// `tol < 0` ⇒ auto-pick (32 for uint8 / int8, scaled by class range
/// for others). Returns a logical mask the same H × W as I.
Value grayconnected(const Value &I, int row, int col, double tol, std::pmr::memory_resource *mr = nullptr);

/// `J = imoverlay(I, BW, color)` — paint BW=true pixels onto I with
/// `color` (1 × 3 RGB triple; native scale: 0..255 for uint8 input,
/// 0..1 for float input). I may be H × W (grayscale → replicated to
/// 3 channels) or H × W × 3. Output is always H × W × 3 uint8.
Value imoverlay(const Value &I, const Value &BW, const Value &color, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
