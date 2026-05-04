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
Value dice(std::pmr::memory_resource *mr, const Value &A, const Value &B);

/// `j = jaccard(BW1, BW2)` — intersection-over-union (IoU).
///   j = |A ∩ B| / |A ∪ B|
/// Same conventions as `dice`. Returns 1 when both masks are empty.
Value jaccard(std::pmr::memory_resource *mr, const Value &A, const Value &B);

/// `BW = boundarymask(L_or_BW [, conn])` — logical mask of pixels
/// that share a connectivity-neighbour with a *different* label
/// (equivalently: a region edge in the label image).
/// `conn` ∈ {4, 8} (default 8). For binary inputs, returns the
/// foreground perimeter, similar to `bwperim`.
Value boundarymask(std::pmr::memory_resource *mr,
                   const Value &L_or_BW, int conn);

/// `idx = label2idx(L)` — column cell array; entry k is a column
/// of MATLAB-1-based linear indices for label k. Pixels labelled 0
/// are excluded.
Value label2idx(std::pmr::memory_resource *mr, const Value &L);

} // namespace numkit::image
