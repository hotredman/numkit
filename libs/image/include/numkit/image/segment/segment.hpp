// libs/image/include/numkit/image/segment/segment.hpp
//
// Image-segmentation utilities and similarity metrics. The
// "deep-learning" entries from MATLAB's segment family
// (segmentAnythingModel, imsegsam, …) are intentionally omitted.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::image {

/// Sørensen–Dice similarity coefficient (`d = dice(BW1, BW2)`).
///
/// @f$ d = 2\,\frac{|A \cap B|}{|A| + |B|} @f$.
/// Operates on binary masks; any non-zero pixel counts as foreground.
/// Returns 1 when both masks are empty (degenerate convention).
///
/// @param A,B  Same-sized binary masks.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Scalar in [0, 1].
///
/// @see jaccard
Value dice(const Value &A, const Value &B,
           std::pmr::memory_resource *mr = nullptr);

/// Intersection-over-union (`j = jaccard(BW1, BW2)`).
///
/// @f$ j = \frac{|A \cap B|}{|A \cup B|} @f$.
/// Same input semantics as @ref dice. Related by
/// @f$ d = 2j / (1 + j) @f$.
///
/// @see dice
Value jaccard(const Value &A, const Value &B,
              std::pmr::memory_resource *mr = nullptr);

/// Boundary mask of a labeled or binary image (`BW = boundarymask(L, conn)`).
///
/// For a label image: marks every pixel that has at least one
/// connectivity-neighbour with a different label (i.e. a region
/// edge).
/// For a binary mask: returns the foreground perimeter, similar to
/// MATLAB's `bwperim`.
///
/// @param L_or_BW  Label image (any integer class) or binary mask.
/// @param conn     Connectivity — 4 or 8 (default 8).
/// @param mr       Memory resource (nullptr → process default).
/// @return         Logical boundary mask of the same H × W.
Value boundarymask(const Value &L_or_BW, int conn,
                   std::pmr::memory_resource *mr = nullptr);

/// Group label-image pixels into per-label index lists
/// (`idx = label2idx(L)`).
///
/// Returns a column cell array. Entry `k` is a column vector of
/// MATLAB-1-based linear indices of all pixels with label `k`. Label
/// 0 (background) is excluded.
///
/// @param L   Integer-typed label image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Cell array of length max(L), each entry a column of indices.
Value label2idx(const Value &L,
                std::pmr::memory_resource *mr = nullptr);

/// Flood-fill region from a seed pixel (`BW = grayconnected(I, row, col, tol)`).
///
/// 8-connected flood-fill: every neighbour whose intensity differs
/// from the seed value by ≤ `tol` is accepted. Coordinates are
/// 1-based (MATLAB convention).
///
/// @param I     Input image.
/// @param row   Seed row (1-based).
/// @param col   Seed column (1-based).
/// @param tol   Intensity tolerance. Pass `< 0` to request the
///              auto-pick (32 for uint8 / int8, scaled by class
///              range for other types).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Logical mask of the connected region.
Value grayconnected(const Value &I, int row, int col, double tol,
                    std::pmr::memory_resource *mr = nullptr);

/// Paint a binary mask onto an image with a colour
/// (`J = imoverlay(I, BW, color)`).
///
/// Foreground pixels of `BW` are replaced with `color` (1×3 RGB
/// triple). Native scale: 0..255 for uint8 input, 0..1 for float
/// input. Grayscale `I` is replicated to 3 channels first. Output is
/// always H × W × 3 uint8.
///
/// @param I      Grayscale or RGB image.
/// @param BW     Logical mask of the same H × W.
/// @param color  1×3 RGB triple.
/// @param mr     Memory resource (nullptr → process default).
/// @return       H × W × 3 uint8 image.
Value imoverlay(const Value &I, const Value &BW, const Value &color,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
