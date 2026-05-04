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

} // namespace numkit::image
