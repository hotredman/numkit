// libs/image/include/numkit/image/texture/texture.hpp
//
// Texture-analysis primitives. The gray-level co-occurrence matrix
// (GLCM) and its property extractors are the work-horses of
// texture analysis.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>

namespace numkit::image {

/// Gray-level co-occurrence matrix (`G = graycomatrix(I, ...)`).
///
/// Quantises `I` into `numLevels` bins over `[gLow, gHigh]`, then for
/// every pixel pair `(p, q = p + (offR, offC))` increments
/// `G[q_level, p_level]`.
///
/// Note: the GLCM convention is that **rows** index the level of
/// the first pixel and **columns** index the offset pixel. When
/// `symmetric` is true, the reverse transition `G[j, i]` is also
/// counted (the `'Symmetric'` option).
///
/// @param I          2-D grayscale image (any numeric class).
/// @param numLevels  Number of quantisation bins (typical 8 for uint8,
///                   2 for logical).
/// @param offR       Row offset of the second pixel.
/// @param offC       Column offset of the second pixel.
/// @param gLow       Lower bound of GrayLimits (class min if NaN).
/// @param gHigh      Upper bound of GrayLimits (class max if NaN).
/// @param symmetric  When true, also count the reverse transition.
/// @param mr         Memory resource (nullptr → process default).
/// @return           `numLevels × numLevels` double matrix of counts.
///
/// **KNOWN GAPS:**
///   - Multiple offsets in one call (would need a 3-D GLCM); pass
///     each offset separately for now.
///   - `'NumLevels'` auto-default for floating-point inputs uses a
///     conservative 8 instead of 64.
///
/// @see graycoprops
Value graycomatrix(const Value &I, int numLevels, int offR, int offC,
                   double gLow, double gHigh, bool symmetric,
                   std::pmr::memory_resource *mr = nullptr);

/// Texture statistics from a GLCM (`S = graycoprops(G)`).
///
/// Given a GLCM `G` (assumed normalised internally to a probability
/// matrix `p = G / sum(G)`), returns a struct Value with the four
/// canonical fields:
///
///   - `Contrast`    @f$ = \sum_{i,j} (i - j)^2\,p(i, j) @f$
///   - `Correlation` @f$ = \sum_{i,j} \frac{(i - \mu_i)(j - \mu_j)\,p(i, j)}{\sigma_i\,\sigma_j} @f$
///   - `Energy`      @f$ = \sum_{i,j} p(i, j)^2 @f$
///   - `Homogeneity` @f$ = \sum_{i,j} \frac{p(i, j)}{1 + |i - j|} @f$
///
/// @param G   GLCM (square matrix from @ref graycomatrix).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Struct Value with fields Contrast, Correlation,
///            Energy, Homogeneity.
///
/// @see graycomatrix
Value graycoprops(const Value &G,
                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
