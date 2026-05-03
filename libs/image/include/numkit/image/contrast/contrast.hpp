// libs/image/include/numkit/image/contrast/contrast.hpp
//
// Histogram-based contrast operations.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

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

/// imquantize(I, levels) — quantise into N+1 classes given N thresholds.
Value imquantize(std::pmr::memory_resource *mr, const Value &I, const Value &levels);

} // namespace numkit::image
