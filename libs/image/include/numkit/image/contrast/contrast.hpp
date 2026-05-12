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
imhist(const Value &I, int n, std::pmr::memory_resource *mr = nullptr);

/// stretchlim(I[, tol]) — returns 2-element column with low/high
/// intensity limits chosen so `tol[0]` fraction of pixels fall below
/// the lower limit and `tol[1]` fraction above the upper limit.
/// Default tol = [0.01, 0.99]. Per-channel for RGB.
Value stretchlim(const Value &I, double low_tol, double high_tol, std::pmr::memory_resource *mr = nullptr);

/// imadjust(I, [low_in high_in], [low_out high_out], gamma) — affine
/// remap with optional gamma. NaN sentinels for low_in/high_in/etc.
/// trigger automatic stretchlim defaults.
Value imadjust(const Value &I, double low_in, double high_in, double low_out, double high_out, double gamma, std::pmr::memory_resource *mr = nullptr);

/// adaptthresh(I [, sensitivity [, neighborhood [, statistic]]]) —
/// locally adaptive threshold matrix in [0, 1] (same scale MATLAB
/// returns). Pair with `imbinarize(I, T)` for adaptive thresholding.
///   sensitivity   ∈ [0, 1], default 0.5. Higher → more pixels are
///                  classified as foreground (lower local threshold).
///   neighborhood   filter size in pixels (odd). Default
///                  2·floor(min(H,W)/16) + 1.
///   statistic     "mean" (default, box filter) or "gaussian" (σ ≈
///                  neighborhood/6).
Value adaptthresh(const Value &I, double sensitivity, int neighborhood, const std::string &statistic, std::pmr::memory_resource *mr = nullptr);

/// histeq(I[, n]) — histogram equalisation with n=64 default bins.
Value histeq(const Value &I, int n, std::pmr::memory_resource *mr = nullptr);

/// Parameters for adapthisteq(). Field defaults match MATLAB R2025b.
struct AdaptHistEqOptions {
    /// NumTiles along the row axis. Image is partitioned into
    /// numTilesR × numTilesC contextual regions.
    int          numTilesR    = 8;

    /// NumTiles along the column axis.
    int          numTilesC    = 8;

    /// Histogram clip fraction in [0, 1]; 0 disables clipping. Higher
    /// values produce more contrast.
    double       clipLimit    = 0.01;

    /// Histogram bin count.
    int          nBins        = 256;

    /// Target histogram shape:
    ///   * `"uniform"`     — flat (default)
    ///   * `"rayleigh"`    — KNOWN GAP, throws
    ///   * `"exponential"` — KNOWN GAP, throws
    std::string  distribution = "uniform";

    /// Shape parameter for rayleigh / exponential (deferred).
    double       alpha        = 0.4;
};

/// adapthisteq(I, ...) — Contrast Limited Adaptive Histogram
/// Equalisation (CLAHE). Divides the image into NumTilesR × NumTilesC
/// regions, builds a clipped-redistribute histogram per tile, and
/// applies the bilinearly-interpolated per-tile CDF as the per-pixel
/// transfer function. Mirrors MATLAB's `adapthisteq`.
///
/// @param I     2-D image (uint8 / uint16 / int16 / single / double).
///              Returned in the same class.
/// @param opts  Algorithm parameters; see AdaptHistEqOptions.
/// @param mr    Memory resource (nullptr → process default).
///
/// KNOWN GAPS:
///   - 3-D / RGB input. MATLAB accepts greyscale only too.
///   - "rayleigh" / "exponential" distributions.
///   - Range='original' option (always 'full' here).
///
/// @code
/// // Default options:
/// Value J1 = adapthisteq(I);
///
/// // Custom options (C++17 field-by-field; switch to designated
/// // initialisers when the project moves to C++20):
/// AdaptHistEqOptions opts;
/// opts.clipLimit = 0.03;
/// opts.numTilesR = 16;
/// opts.numTilesC = 16;
/// Value J2 = adapthisteq(I, opts);
/// @endcode
Value adapthisteq(const Value &                I,
                  const AdaptHistEqOptions &   opts = {},
                  std::pmr::memory_resource *  mr   = nullptr);

// ── Thresholding ──────────────────────────────────────────────────────

/// graythresh(I) — Otsu's threshold + effectiveness metric.
/// Returns (threshold ∈ [0, 1], em ∈ [0, 1]).
std::tuple<Value, Value>
graythresh(const Value &I, std::pmr::memory_resource *mr = nullptr);

/// otsuthresh(counts) — Otsu's threshold from a histogram. Returns
/// (threshold ∈ [0, 1], em).
std::tuple<Value, Value>
otsuthresh(const Value &counts, std::pmr::memory_resource *mr = nullptr);

/// multithresh(I[, N]) — N-level Otsu. Returns (N thresholds, em).
std::tuple<Value, Value>
multithresh(const Value &I, int N, std::pmr::memory_resource *mr = nullptr);

/// imbinarize(I[, thresh]) — apply a threshold (default = graythresh).
Value imbinarize(const Value &I, double thresh, std::pmr::memory_resource *mr = nullptr);

/// imbinarize(I, T) — per-pixel threshold; T must have the same numel
/// as I. Composes naturally with `adaptthresh(I, …)`.
Value imbinarize(const Value &I, const Value &T, std::pmr::memory_resource *mr = nullptr);

/// imquantize(I, levels) — quantise into N+1 classes given N thresholds.
Value imquantize(const Value &I, const Value &levels, std::pmr::memory_resource *mr = nullptr);

/// imhistmatch(I, ref [, nbins]) — adjust I's histogram to match
/// the reference image's. CDF-matching algorithm. Single-channel.
/// nbins default 64 for double/single, 256 for uint8, 65536 for
/// uint16. Output has the same class as I.
Value imhistmatch(const Value &I, const Value &ref, int nbins, std::pmr::memory_resource *mr = nullptr);

/// imflatfield(I, sigma [, mask]) — flat-field correction. Divide
/// the image by a low-pass estimate of its background and rescale
/// so the mean is preserved:
///   F = imgaussfilt(I_double, sigma)
///   B = I_double ./ F .* mean(F[mask])
/// Output is cast back to the input class with saturation. The
/// optional mask restricts the mean-of-F average.
Value imflatfield(const Value &I, double sigma, const Value &mask, std::pmr::memory_resource *mr = nullptr);

/// `Y = wcodemat(X [, nb [, opt [, absol]]])` — quantize and scale
/// `X` into integer codes in [1, nb]. `opt` ∈ {"mat" (default,
/// global), "row", "col"}; `absol` controls whether to use abs(X)
/// (1, default) or X (0). Output is double; this is the canonical
/// wavelet-display helper from MATLAB's Wavelet Toolbox.
Value wcodemat(const Value &X, int nb, const std::string &opt, int absol, std::pmr::memory_resource *mr = nullptr);

/// entropy(I [, nbins]) — Shannon entropy of the image histogram in
/// bits. For non-logical images the image is first converted to
/// uint8 with 256 bins by default; logical images use 2 bins. Zero
/// counts are skipped before the log2 sum.
Value entropy(const Value &I, int nbins, std::pmr::memory_resource *mr = nullptr);

/// grayslice(I, n) — multilevel thresholding into an indexed image.
///   - n scalar ≥ 1: thresholds at (1/n, 2/n, …, (n-1)/n) of the
///     image's class range.
///   - n a vector or 0 < n < 1: explicit threshold values; for
///     floating-point images the vector is clamped to [min(I) max(I)]
///     (extending toward image bounds, never shrinking).
/// Output is uint8 if the number of levels is < 256, else double + 1
/// (1-based indexing per MATLAB).
Value grayslice(const Value &I, const Value &n, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
