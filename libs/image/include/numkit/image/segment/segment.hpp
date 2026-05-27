// libs/image/include/numkit/image/segment/segment.hpp
//
// Image-segmentation utilities and similarity metrics. The
// "deep-learning" segmentation entries (segmentAnythingModel,
// imsegsam, …) are intentionally omitted.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::image {

/// @brief Sørensen–Dice similarity coefficient
/// (`d = dice(BW1, BW2)`).
///
/// `d = 2·|A ∩ B| / (|A| + |B|)`. Operates on binary masks; any
/// non-zero pixel counts as foreground. Returns 1 when both masks
/// are empty (degenerate convention).
///
/// @param A   Same-sized binary mask.
/// @param B   Same-sized binary mask.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar in `[0, 1]`.
/// @see jaccard
Value dice(const Value &A, const Value &B,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Intersection-over-union (`j = jaccard(BW1, BW2)`).
///
/// `j = |A ∩ B| / |A ∪ B|`. Same input semantics as @ref dice.
/// Related by `d = 2j / (1 + j)`.
///
/// @param A   Same-sized binary mask.
/// @param B   Same-sized binary mask.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar in `[0, 1]`.
/// @see dice
Value jaccard(const Value &A, const Value &B,
              std::pmr::memory_resource *mr = nullptr);

/// Boundary mask of a labeled or binary image (`BW = boundarymask(L, conn)`).
///
/// For a label image: marks every pixel that has at least one
/// connectivity-neighbour with a different label (i.e. a region
/// edge).
/// For a binary mask: returns the foreground perimeter, similar to
/// the `bwperim` operation.
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
/// 1-based linear indices of all pixels with label `k`. Label
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
/// 1-based.
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

/// @brief Pixel weights from grayscale intensity difference
/// (`W = graydiffweight(I, refGrayVal [, NV...])`).
///
/// Used by Fast-Marching-Method-based segmentation (`imsegfmm`).
/// For each pixel: `d = |I − refGrayVal|`; linearly scale `d` to
/// `[1e-3, 1]`; then `W = 1 / (d^(1/RolloffFactor))`. Pixels close
/// to the reference get a very large weight (~10^6 with the default
/// `RolloffFactor = 0.5`); far pixels get weight 1.
///
/// **Reference value source.** This typed entry-point takes a scalar
/// `refGrayVal`; the engine adapter additionally exposes the four
/// MATLAB R2025b signatures
///   `graydiffweight(I, refGrayVal)`,
///   `graydiffweight(I, MASK)`               — mean of `I(MASK)`,
///   `graydiffweight(I, C, R)`               — mean at linear (R, C),
///   `graydiffweight(V, C, R, P)`            — 3-D version,
/// each of which resolves to a single scalar reference internally.
///
/// **Options:**
///   * `RolloffFactor` (default 0.5) — controls how fast `W` falls.
///   * `GrayDifferenceCutoff` (default `+Inf`) — when finite, any
///     pixel with `d > cutoff` (BEFORE scaling) is forced to 1
///     (largest scaled-`d`, smallest output weight).
///
/// Output class: `single` if `I` is `single`, else `double` (matches
/// MATLAB R2025b).
///
/// @param I              Grayscale 2-D / 3-D image (any numeric class).
/// @param ref_gray_val   Reference intensity (scalar).
/// @param rolloff_factor `> 0`. Default 0.5.
/// @param cutoff         `>= 0` or `+Inf` (no cutoff). Default `+Inf`.
/// @param mr             Memory resource (nullptr → process default).
/// @return               Weight array (same shape as `I`).
Value graydiffweight(const Value &I, double ref_gray_val,
                     double rolloff_factor, double cutoff,
                     std::pmr::memory_resource *mr = nullptr);

/// @brief Pixel weights from image gradient magnitude
/// (`W = gradientweight(I [, sigma], 'RolloffFactor', P, 'WeightCutoff', K)`).
///
/// Companion to @ref graydiffweight for Fast-Marching-Method based
/// segmentation (`imsegfmm`): output is large in smooth regions and
/// small on edges, so the FMM front travels fast inside objects and
/// stalls at boundaries.
///
/// Algorithm (transliterated from MATLAB R2025b
/// `images.internal.imgradientdog` + `gradientweight.m`):
///
///   1. Derivative-of-Gaussian (DoG) kernels of radius
///      `r = ceil(2σ)` along each axis:
///        `hx(x) = -x · exp(-x²/(2σ²))` for `x ∈ {-r,...,r}`,
///        normalised so the positive half (`hx(1..r)`) sums to 1.
///        `hy` is the column-transpose with `σ_y`.
///   2. `Gx = imfilter(I, hx, 'replicate')`,
///      `Gy = imfilter(I, hy, 'replicate')`,
///      `Gmag = hypot(Gx, Gy)`.
///   3. `W = imlinscale(Gmag, [0, 1])`              (linear rescale).
///   4. `W = W .^ (1 / RolloffFactor)`,
///      `W = (1 − W) ./ (1 + W)`.
///   5. `W(W < WeightCutoff) = 1e-3`               (FMM stop floor).
///
/// `σ` may be a scalar (replicated per dim) or a 2-element vector
/// `[σ_x, σ_y]`. Constant-image fast-path returns `ones(size(I))`.
///
/// Output class: `single` if `I` is `single`, else `double`
/// (matches MATLAB R2025b — uint8/int8/etc → double).
///
/// 2-D inputs only (3-D throws — MATLAB calls `imgradientdog3` for
/// volumes; not yet ported).
///
/// References: Gonzalez & Woods, *Digital Image Processing*, §10
/// (edge detection via gradient magnitude); Sethian, *Level Set
/// Methods and Fast Marching Methods*, Cambridge 1999 (FMM weights).
///
/// @param I              2-D grayscale image (any numeric class).
/// @param sigma_x        DoG σ along x (cols). Must be `> 0`.
/// @param sigma_y        DoG σ along y (rows). Must be `> 0`.
/// @param rolloff_factor `> 0`. Default 3. Controls falloff steepness.
/// @param weight_cutoff  `∈ [1e-3, 1]`. Default 0.25. Threshold floor.
/// @param mr             Memory resource (nullptr → process default).
/// @return               Weight array, same H×W as `I`.
Value gradientweight(const Value &I, double sigma_x, double sigma_y,
                     double rolloff_factor, double weight_cutoff,
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
