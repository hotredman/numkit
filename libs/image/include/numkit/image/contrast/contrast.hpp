// libs/image/include/numkit/image/contrast/contrast.hpp
//
// Histogram-based contrast operations.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::image {

/// imhist(I[, n]) — histogram. Returns (counts, bin_centers).
/// Default n: 256 for uint8, 65536 for uint16, 64 for double in [0, 1].
std::tuple<Value, Value>
imhist(std::pmr::memory_resource *mr, const Value &I, int n);

/// stretchlim(I[, tol]) — returns 2-element column with low/high
/// intensity limits chosen so `tol[0]` fraction of pixels fall below
/// the lower limit and `tol[1]` fraction above the upper limit.
/// Default tol = [0.01, 0.99]. Per-channel for RGB.
Value stretchlim(std::pmr::memory_resource *mr, const Value &I,
                 double low_tol, double high_tol);

/// imadjust(I, [low_in high_in], [low_out high_out], gamma) — affine
/// remap with optional gamma. NaN sentinels for low_in/high_in/etc.
/// trigger automatic stretchlim defaults.
Value imadjust(std::pmr::memory_resource *mr, const Value &I,
               double low_in, double high_in,
               double low_out, double high_out,
               double gamma);

/// adaptthresh(I [, sensitivity [, neighborhood [, statistic]]]) —
/// locally adaptive threshold matrix in [0, 1] (same scale MATLAB
/// returns). Pair with `imbinarize(I, T)` for adaptive thresholding.
///   sensitivity   ∈ [0, 1], default 0.5. Higher → more pixels are
///                  classified as foreground (lower local threshold).
///   neighborhood   filter size in pixels (odd). Default
///                  2·floor(min(H,W)/16) + 1.
///   statistic     "mean" (default, box filter) or "gaussian" (σ ≈
///                  neighborhood/6).
Value adaptthresh(std::pmr::memory_resource *mr, const Value &I,
                  double sensitivity, int neighborhood,
                  const std::string &statistic);

/// histeq(I[, n]) — histogram equalisation with n=64 default bins.
Value histeq(std::pmr::memory_resource *mr, const Value &I, int n);

// ── Thresholding ──────────────────────────────────────────────────────

/// graythresh(I) — Otsu's threshold + effectiveness metric.
/// Returns (threshold ∈ [0, 1], em ∈ [0, 1]).
std::tuple<Value, Value>
graythresh(std::pmr::memory_resource *mr, const Value &I);

/// otsuthresh(counts) — Otsu's threshold from a histogram. Returns
/// (threshold ∈ [0, 1], em).
std::tuple<Value, Value>
otsuthresh(std::pmr::memory_resource *mr, const Value &counts);

/// multithresh(I[, N]) — N-level Otsu. Returns (N thresholds, em).
std::tuple<Value, Value>
multithresh(std::pmr::memory_resource *mr, const Value &I, int N);

/// imbinarize(I[, thresh]) — apply a threshold (default = graythresh).
Value imbinarize(std::pmr::memory_resource *mr, const Value &I, double thresh);

/// imbinarize(I, T) — per-pixel threshold; T must have the same numel
/// as I. Composes naturally with `adaptthresh(I, …)`.
Value imbinarize(std::pmr::memory_resource *mr, const Value &I,
                 const Value &T);

/// imquantize(I, levels) — quantise into N+1 classes given N thresholds.
Value imquantize(std::pmr::memory_resource *mr, const Value &I, const Value &levels);

/// imhistmatch(I, ref [, nbins]) — adjust I's histogram to match
/// the reference image's. CDF-matching algorithm. Single-channel.
/// nbins default 64 for double/single, 256 for uint8, 65536 for
/// uint16. Output has the same class as I.
Value imhistmatch(std::pmr::memory_resource *mr,
                  const Value &I, const Value &ref, int nbins);

/// imflatfield(I, sigma [, mask]) — flat-field correction. Divide
/// the image by a low-pass estimate of its background and rescale
/// so the mean is preserved:
///   F = imgaussfilt(I_double, sigma)
///   B = I_double ./ F .* mean(F[mask])
/// Output is cast back to the input class with saturation. The
/// optional mask restricts the mean-of-F average.
Value imflatfield(std::pmr::memory_resource *mr,
                  const Value &I, double sigma, const Value &mask);

} // namespace numkit::image
