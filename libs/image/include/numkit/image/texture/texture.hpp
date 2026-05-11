// libs/image/include/numkit/image/texture/texture.hpp
//
// Texture-analysis primitives. The gray-level co-occurrence matrix
// (GLCM) and its property extractors are the work-horses of
// MATLAB's Texture Analysis tooling.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::image {

/// graycomatrix(I[, NV-pairs]) — Gray-level co-occurrence matrix.
///
/// Quantises I into `numLevels` bins over [gLow, gHigh], then for each
/// pixel pair (p, q = p + (offR, offC)) increments G[ q_level, p_level ]
/// — note MATLAB's GLCM rows index the FIRST pixel and columns the
/// OFFSET pixel. When `symmetric` is true, G[ j, i ] is also
/// incremented (matches MATLAB's 'Symmetric' option).
///
/// @param I          2-D grayscale image (any numeric class).
/// @param numLevels  Number of quantization bins (default 8 for uint8,
///                   2 for logical, 8 otherwise).
/// @param offR       Row offset of the second pixel.
/// @param offC       Column offset of the second pixel.
/// @param gLow       Lower bound of GrayLimits (default = class min).
/// @param gHigh      Upper bound of GrayLimits (default = class max).
/// @param symmetric  When true, also count the reverse transition.
/// @returns          A numLevels × numLevels matrix of double counts.
///
/// KNOWN GAPS:
///   - Multiple offsets in one call (returns a 3-D GLCM in MATLAB).
///     Pass each offset separately for now.
///   - 'NumLevels' auto-default for floating-point inputs uses a
///     conservative 8 instead of 64.
Value graycomatrix(std::pmr::memory_resource *mr,
                   const Value &I,
                   int numLevels,
                   int offR, int offC,
                   double gLow, double gHigh,
                   bool symmetric);

/// graycoprops(G) — Texture statistics from a GLCM:
///   .Contrast     = sum (i - j)² · p(i, j)
///   .Correlation  = sum (i - μ_i)(j - μ_j) · p(i, j) / (σ_i · σ_j)
///   .Energy       = sum p(i, j)²
///   .Homogeneity  = sum p(i, j) / (1 + |i - j|)
/// where p is the joint probability (GLCM normalised by its sum).
/// MATLAB's `graycoprops` returns a struct; this returns a struct
/// `Value` with the same four field names.
Value graycoprops(std::pmr::memory_resource *mr, const Value &G);

} // namespace numkit::image
