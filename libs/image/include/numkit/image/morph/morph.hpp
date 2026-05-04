// libs/image/include/numkit/image/morph/morph.hpp
//
// Morphological operations. Function-form only: `strel` returns a
// numeric structuring-element matrix (LOGICAL HxW), and the operators
// accept either that matrix or any HxW logical / numeric mask.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <vector>

namespace numkit::image {

/// strel(shape[, params]) — build a 2-D structuring-element mask.
/// Supported shapes:
///   "square"    — N×N
///   "rectangle" — [m, n]
///   "diamond"   — radius r
///   "disk"      — radius r (decomposed approximation)
///   "line"      — length L at angle θ degrees
///   "arbitrary" / "" — caller-provided HxW mask (passed via params as
///                      flat row-major doubles + dims[0], dims[1] in params)
/// Returns a LOGICAL matrix (1 = element present in neighbourhood).
Value strel(std::pmr::memory_resource *mr,
            const std::string &shape,
            const std::vector<double> &params,
            const Value &arbitrary_nhood);

/// imerode(I, SE) — grayscale erosion (output = min within SE
/// neighbourhood). For LOGICAL input this reduces to the binary
/// definition (pixel = 1 iff all SE-marked pixels are 1).
Value imerode(std::pmr::memory_resource *mr, const Value &I, const Value &SE);

/// imdilate(I, SE) — grayscale dilation (output = max within SE).
Value imdilate(std::pmr::memory_resource *mr, const Value &I, const Value &SE);

/// imopen(I, SE)  = imdilate(imerode(I, SE), SE).
Value imopen(std::pmr::memory_resource *mr, const Value &I, const Value &SE);

/// imclose(I, SE) = imerode(imdilate(I, SE), SE).
Value imclose(std::pmr::memory_resource *mr, const Value &I, const Value &SE);

/// `J = imreconstruct(marker, mask [, conn])` — morphological
/// reconstruction by dilation. Iteratively dilates `marker` and
/// caps it elementwise against `mask` until stable:
///   J_{k+1} = min(imdilate(J_k, SE), mask),  J_0 = marker
/// Works on binary or grayscale inputs; `marker ≤ mask` element-wise
/// is required (we clip to the mask up-front to enforce it).
/// `conn` ∈ {4, 8} (default 8). Same H × W output type as input.
Value imreconstruct(std::pmr::memory_resource *mr,
                    const Value &marker, const Value &mask, int conn);

/// `J = imfill(BW, 'holes' [, conn])` — fill interior holes in a
/// binary image. Holes are 0-pixels NOT connectivity-reachable from
/// the image border. Implementation:
///   marker = ~BW restricted to the image border
///   R      = imreconstruct(marker, ~BW, conn)   // border-touching bg
///   J      = ~R                                 // foreground + holes
Value imfill_holes(std::pmr::memory_resource *mr,
                   const Value &BW, int conn);

/// `J = imregionalmax(I [, conn])` — logical mask of regional maxima
/// (connected pixel groups whose neighbours are all strictly smaller).
/// Standard formula:  regmax(I) = (I − imreconstruct(I − 1, I)) > 0.
Value imregionalmax(std::pmr::memory_resource *mr,
                    const Value &I, int conn);

/// `J = imregionalmin(I [, conn])` — dual of imregionalmax: regional
/// minima of I via inversion (`(typeMax − I)` for unsigned, `−I` for
/// floats) → imregionalmax → return.
Value imregionalmin(std::pmr::memory_resource *mr,
                    const Value &I, int conn);

/// `J = imhmax(I, h [, conn])` — h-maxima transform. Suppresses
/// regional maxima shallower than `h` units. Formula:
///   imhmax(I, h) = imreconstruct(I − h, I, conn)
/// Composes with imregionalmax to count only "deep" peaks.
Value imhmax(std::pmr::memory_resource *mr,
             const Value &I, double h, int conn);

/// `J = imhmin(I, h [, conn])` — h-minima transform; dual of imhmax
/// via image inversion.
Value imhmin(std::pmr::memory_resource *mr,
             const Value &I, double h, int conn);

/// `J = imextendedmax(I, h [, conn])` — extended-maxima transform.
/// Logical mask of regional maxima of imhmax(I, h): keeps only peaks
/// with height ≥ h above their surroundings. One-liner:
///   imextendedmax(I, h) = imregionalmax(imhmax(I, h))
Value imextendedmax(std::pmr::memory_resource *mr,
                    const Value &I, double h, int conn);

/// `J = imextendedmin(I, h [, conn])` — extended-minima transform;
/// dual of imextendedmax. Logical mask of regional minima of
/// imhmin(I, h): keeps only troughs with depth ≥ h.
Value imextendedmin(std::pmr::memory_resource *mr,
                    const Value &I, double h, int conn);

/// `J = imimposemin(I, BW [, conn])` — minima imposition. Modify
/// grayscale `I` so its only regional minima sit at the marker
/// pixels in BW. Soille's reconstruction-by-erosion recipe:
///   marker fm = -∞ at BW, +∞ elsewhere
///   mask    m = min(I, fm) = -∞ at BW, I elsewhere
///   J         = R^E_m(fm) — fill non-marker basins up to their
///               boundary, keep markers at the global floor.
Value imimposemin(std::pmr::memory_resource *mr,
                  const Value &I, const Value &BW, int conn);

/// `J = imclearborder(BW [, conn])` — remove connected components
/// that touch the image border. Standard reconstruction recipe:
///   marker = BW restricted to the rim
///   R      = imreconstruct(marker, BW, conn)   // border-touching FG
///   J      = BW & ~R
/// Output is logical; same H × W as input.
Value imclearborder(std::pmr::memory_resource *mr,
                    const Value &BW, int conn);

/// `J = imkeepborder(BW [, conn])` — exact dual of imclearborder:
/// keep ONLY components that touch the image rim.
///   marker = BW restricted to the rim
///   J      = imreconstruct(marker, BW, conn)
Value imkeepborder(std::pmr::memory_resource *mr,
                   const Value &BW, int conn);

/// `J = imtophat(I, SE)` — white top-hat: I − imopen(I, SE).
/// Highlights bright structures smaller than SE on a dark
/// background. Output saturates to the input class.
Value imtophat(std::pmr::memory_resource *mr,
               const Value &I, const Value &SE);

/// `J = imbothat(I, SE)` — black top-hat: imclose(I, SE) − I.
/// Highlights dark structures smaller than SE on a bright
/// background. Output saturates to the input class.
Value imbothat(std::pmr::memory_resource *mr,
               const Value &I, const Value &SE);

} // namespace numkit::image
