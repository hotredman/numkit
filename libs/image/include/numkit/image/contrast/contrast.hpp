// libs/image/include/numkit/image/contrast/contrast.hpp
//
// Histogram-based contrast operations.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::image {

/// Image histogram (`[counts, bin_centers] = imhist(I, n)`).
///
/// Default `n` depends on input class: 256 for uint8, 65536 for
/// uint16, 64 for double in [0, 1].
///
/// @param I   Input image (any numeric class).
/// @param n   Bin count.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(counts, centers)`.
std::tuple<Value, Value>
imhist(const Value &I, int n, std::pmr::memory_resource *mr = nullptr);

/// Stretch limits from saturation tolerances
/// (`lim = stretchlim(I, [low_tol, high_tol])`).
///
/// Returns a 2-element column with the low / high intensities chosen
/// so `low_tol` fraction of pixels fall below the lower limit and
/// `1 − high_tol` fraction above the upper limit. Default
/// `low_tol = 0.01`, `high_tol = 0.99`. Computed per channel for RGB.
///
/// @see imadjust
Value stretchlim(const Value &I, double low_tol, double high_tol,
                 std::pmr::memory_resource *mr = nullptr);

/// Affine intensity remap with optional gamma (`J = imadjust(...)`).
///
/// Implements MATLAB's `imadjust(I, [low_in high_in], [low_out high_out], gamma)`.
/// Pass `NaN` for `low_in` / `high_in` to trigger an automatic
/// @ref stretchlim default; pass NaN for the output limits to map
/// to `[0, 1]`.
///
/// @param I         Input image.
/// @param low_in    Low input intensity (NaN → auto via stretchlim).
/// @param high_in   High input intensity (NaN → auto via stretchlim).
/// @param low_out   Low output intensity (NaN → 0).
/// @param high_out  High output intensity (NaN → 1).
/// @param gamma     Gamma exponent (1.0 = linear, > 1 darkens).
/// @param mr        Memory resource (nullptr → process default).
/// @return          Remapped image of the same class as `I`.
///
/// @see histeq, adapthisteq
Value imadjust(const Value &I,
               double low_in, double high_in,
               double low_out, double high_out,
               double gamma,
               std::pmr::memory_resource *mr = nullptr);

/// Locally adaptive threshold matrix (`T = adaptthresh(I, ...)`).
///
/// Computes a per-pixel threshold matrix in [0, 1] (same scale that
/// MATLAB returns). Pair with `imbinarize(I, T)` for adaptive
/// thresholding.
///
/// @param I             Input image.
/// @param sensitivity   In [0, 1], default 0.5. Higher → more pixels
///                      classified as foreground (lower local threshold).
/// @param neighborhood  Filter size in pixels (odd). Default
///                      `2·floor(min(H,W)/16) + 1`.
/// @param statistic     `"mean"` (default, box filter) or
///                      `"gaussian"` (σ ≈ neighborhood/6).
/// @param mr            Memory resource (nullptr → process default).
/// @return              Threshold matrix in [0, 1].
///
/// @see imbinarize, graythresh
Value adaptthresh(const Value &I, double sensitivity, int neighborhood,
                  const std::string &statistic,
                  std::pmr::memory_resource *mr = nullptr);

/// Histogram equalisation (`J = histeq(I, n)`).
///
/// Equalises the image histogram across `n` bins (default 64).
///
/// @see adapthisteq
Value histeq(const Value &I, int n,
             std::pmr::memory_resource *mr = nullptr);

/// Parameters for @ref adapthisteq. Field defaults match MATLAB R2025b.
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
    ///   - `"uniform"`     — flat (default)
    ///   - `"rayleigh"`    — KNOWN GAP, throws
    ///   - `"exponential"` — KNOWN GAP, throws
    std::string  distribution = "uniform";

    /// Shape parameter for rayleigh / exponential (deferred).
    double       alpha        = 0.4;
};

/// Contrast Limited Adaptive Histogram Equalisation
/// (`J = adapthisteq(I, opts)`).
///
/// Divides the image into `numTilesR × numTilesC` regions, builds a
/// clipped-redistribute histogram per tile, and applies the
/// bilinearly-interpolated per-tile CDF as the per-pixel transfer
/// function. Mirrors MATLAB's `adapthisteq`.
///
/// @param I     2-D image (uint8 / uint16 / int16 / single / double).
///              Returned in the same class.
/// @param opts  Algorithm parameters; see @ref AdaptHistEqOptions.
/// @param mr    Memory resource (nullptr → process default).
///
/// **KNOWN GAPS:**
///   - 3-D / RGB input. MATLAB accepts greyscale only too.
///   - `"rayleigh"` / `"exponential"` distributions.
///   - `Range='original'` option (always 'full' here).
///
/// @code
/// Value J1 = adapthisteq(I);                   // defaults
/// AdaptHistEqOptions opts;
/// opts.clipLimit = 0.03;
/// opts.numTilesR = 16;
/// opts.numTilesC = 16;
/// Value J2 = adapthisteq(I, opts);
/// @endcode
///
/// @see histeq, imadjust
Value adapthisteq(const Value &                I,
                  const AdaptHistEqOptions &   opts = {},
                  std::pmr::memory_resource *  mr   = nullptr);

// ── Thresholding ──────────────────────────────────────────────────────

/// Otsu's threshold and effectiveness metric
/// (`[level, em] = graythresh(I)`).
///
/// Returns `(threshold, em)` both in [0, 1].
///
/// @see otsuthresh, multithresh, imbinarize
std::tuple<Value, Value>
graythresh(const Value &I, std::pmr::memory_resource *mr = nullptr);

/// Otsu's threshold from a precomputed histogram
/// (`[level, em] = otsuthresh(counts)`).
///
/// @see graythresh
std::tuple<Value, Value>
otsuthresh(const Value &counts,
           std::pmr::memory_resource *mr = nullptr);

/// N-level Otsu (`[thresh, em] = multithresh(I, N)`).
///
/// Returns N thresholds and the global effectiveness metric.
std::tuple<Value, Value>
multithresh(const Value &I, int N,
            std::pmr::memory_resource *mr = nullptr);

/// Threshold an image (`BW = imbinarize(I, thresh)`).
///
/// Scalar-threshold overload. Pass `NaN` for `thresh` to auto-pick
/// via @ref graythresh.
Value imbinarize(const Value &I, double thresh,
                 std::pmr::memory_resource *mr = nullptr);

/// Per-pixel threshold (`BW = imbinarize(I, T)`).
///
/// `T` must have the same `numel` as `I`. Composes naturally with
/// @ref adaptthresh.
Value imbinarize(const Value &I, const Value &T,
                 std::pmr::memory_resource *mr = nullptr);

/// Quantise into N+1 classes (`L = imquantize(I, levels)`).
///
/// `levels` is an N-vector of thresholds; output is the class index
/// (1-based) of each pixel.
Value imquantize(const Value &I, const Value &levels,
                 std::pmr::memory_resource *mr = nullptr);

/// Histogram matching to a reference image
/// (`J = imhistmatch(I, ref, nbins)`).
///
/// Adjusts `I`'s histogram to match `ref`'s via CDF matching
/// (single-channel). `nbins` default: 64 for double/single, 256 for
/// uint8, 65536 for uint16. Output class matches `I`.
Value imhistmatch(const Value &I, const Value &ref, int nbins,
                  std::pmr::memory_resource *mr = nullptr);

/// Flat-field correction (`J = imflatfield(I, sigma, mask)`).
///
/// Divides the image by a low-pass estimate of its background and
/// rescales so the mean is preserved:
///   `F = imgaussfilt(I_double, sigma)`,
///   `J = I_double ./ F .* mean(F[mask])`.
/// Output is cast back to the input class with saturation. The
/// optional `mask` restricts the mean-of-F average.
///
/// @param I      Input image.
/// @param sigma  Gaussian σ (px) for the background estimate.
/// @param mask   Optional logical mask (pass `Value::Empty` to skip).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Corrected image, same class as `I`.
Value imflatfield(const Value &I, double sigma, const Value &mask,
                  std::pmr::memory_resource *mr = nullptr);

/// Wavelet-display quantisation (`Y = wcodemat(X, nb, opt, absol)`).
///
/// Quantises and scales `X` into integer codes in [1, nb].
/// `opt` ∈ {`"mat"` (default, global), `"row"`, `"col"`}.
/// `absol` controls whether to use `abs(X)` (1, default) or `X` (0).
/// Output is double; this is the canonical wavelet-display helper
/// from MATLAB's Wavelet Toolbox.
Value wcodemat(const Value &X, int nb, const std::string &opt, int absol,
               std::pmr::memory_resource *mr = nullptr);

/// Shannon entropy of the image histogram (`H = entropy(I, nbins)`).
///
/// In bits. For non-logical images the image is first converted to
/// uint8 with 256 bins by default; logical images use 2 bins. Zero
/// counts are skipped before the log₂ sum.
Value entropy(const Value &I, int nbins,
              std::pmr::memory_resource *mr = nullptr);

/// Multilevel thresholding into an indexed image
/// (`L = grayslice(I, n)`).
///
/// - `n` scalar ≥ 1: thresholds at `(1/n, 2/n, …, (n-1)/n)` of the
///   image's class range.
/// - `n` a vector or `0 < n < 1`: explicit threshold values; for
///   floating-point images the vector is clamped to `[min(I), max(I)]`
///   (extending toward image bounds, never shrinking).
///
/// Output is uint8 if the number of levels < 256, else `double + 1`
/// (1-based indexing per MATLAB).
Value grayslice(const Value &I, const Value &n,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
