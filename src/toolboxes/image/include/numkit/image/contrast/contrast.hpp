/// @file contrast.hpp
/// @ingroup group_image
// toolboxes/image/include/numkit/image/contrast/contrast.hpp
// Histogram-based contrast operations.

#pragma once

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <string>
#include <tuple>

namespace numkit::image {

/// @addtogroup group_image
/// @{


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

/// @brief Stretch limits from saturation tolerances
/// (`lim = stretchlim(I, [low_tol, high_tol])`).
///
/// Returns a 2-element column with the low / high intensities chosen
/// so `low_tol` fraction of pixels fall below the lower limit and
/// `1 − high_tol` fraction above the upper limit. Default
/// `low_tol = 0.01`, `high_tol = 0.99`. Computed per channel for RGB.
///
/// @param I         Input image.
/// @param low_tol   Lower-tail fraction in [0, 1].
/// @param high_tol  Upper-tail cutoff in [0, 1].
/// @param mr        Memory resource (nullptr → process default).
/// @return          2-element column `[low, high]` (or 2×3 for RGB).
/// @see imadjust
Value stretchlim(const Value &I, double low_tol, double high_tol,
                 std::pmr::memory_resource *mr = nullptr);

/// Affine intensity remap with optional gamma (`J = imadjust(...)`).
///
/// Implements `imadjust(I, [low_in high_in], [low_out high_out], gamma)`.
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
/// Computes a per-pixel threshold matrix in [0, 1]. Pair with
/// `imbinarize(I, T)` for adaptive
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

/// @brief Histogram equalisation (`J = histeq(I, n)`).
///
/// Equalises the image histogram across `n` bins (default 64).
///
/// @param I   Input image.
/// @param n   Number of histogram bins (default 64).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Equalised image, same class as `I`.
/// @see adapthisteq, imadjust
Value histeq(const Value &I, int n,
             std::pmr::memory_resource *mr = nullptr);

/// Parameters for @ref adapthisteq.
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
    ///   - `"rayleigh"`    — bell-shaped (Rayleigh inverse CDF)
    ///   - `"exponential"` — curved (exponential inverse CDF)
    std::string  distribution = "uniform";

    /// Shape parameter for the rayleigh / exponential distributions.
    double       alpha        = 0.4;

    /// Output intensity range:
    ///   - `"full"`     — full range of the image class (default)
    ///   - `"original"` — limited to the input's actual [min, max]
    std::string  range        = "full";
};

/// Contrast Limited Adaptive Histogram Equalisation
/// (`J = adapthisteq(I, opts)`).
///
/// Divides the image into `numTilesR × numTilesC` regions, builds a
/// clipped-redistribute histogram per tile, and applies the
/// bilinearly-interpolated per-tile CDF as the per-pixel transfer
/// function. Full argument set: NumTiles, ClipLimit, NBins, Range,
/// Distribution, Alpha.
///
/// Clean-room implementation from public references (CLAHE — Zuiderveld
/// 1994; contrast limiting — Pizer et al. 1990; non-uniform target
/// distributions — Pizer et al. 1987). See.
///
/// @param I     2-D greyscale image. Returned in the same class.
/// @param opts  Algorithm parameters; see @ref AdaptHistEqOptions.
/// @param mr    Memory resource (nullptr → process default).
///
/// @note 2-D greyscale input only; RGB / N-D input throws
///       `m:adapthisteq:unsupportedShape`.
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

/// @brief Otsu's threshold and effectiveness metric
/// (`[level, em] = graythresh(I)`).
///
/// Returns `(threshold, em)` both in [0, 1].
///
/// @param I   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(level, em)`.
/// @see otsuthresh, multithresh, imbinarize
std::tuple<Value, Value>
graythresh(const Value &I, std::pmr::memory_resource *mr = nullptr);

/// @brief Otsu's threshold from a precomputed histogram
/// (`[level, em] = otsuthresh(counts)`).
///
/// @param counts  Histogram counts (length L vector).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(level, em)` both in [0, 1].
/// @see graythresh
std::tuple<Value, Value>
otsuthresh(const Value &counts,
           std::pmr::memory_resource *mr = nullptr);

/// @brief N-level Otsu (`[thresh, em] = multithresh(I, N)`).
///
/// Returns `N` thresholds and the global effectiveness metric.
///
/// @param I   Input image.
/// @param N   Number of thresholds.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(thresh, em)`.
/// @see graythresh
std::tuple<Value, Value>
multithresh(const Value &I, int N,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Threshold an image (`BW = imbinarize(I, thresh)`).
///
/// Scalar-threshold overload. Pass `NaN` for `thresh` to auto-pick
/// via @ref graythresh.
///
/// @param I       Input image.
/// @param thresh  Scalar threshold (NaN → auto via graythresh).
/// @param mr      Memory resource (nullptr → process default).
/// @return        LOGICAL mask, same shape as `I`.
/// @see graythresh, adaptthresh
Value imbinarize(const Value &I, double thresh,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Per-pixel threshold (`BW = imbinarize(I, T)`).
///
/// `T` must have the same `numel` as `I`. Composes naturally with
/// @ref adaptthresh.
///
/// @param I   Input image.
/// @param T   Per-pixel threshold matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    LOGICAL mask, same shape as `I`.
/// @see adaptthresh
Value imbinarize(const Value &I, const Value &T,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Quantise into N+1 classes (`L = imquantize(I, levels)`).
///
/// `levels` is an N-vector of thresholds; output is the class index
/// (1-based) of each pixel.
///
/// @param I       Input image.
/// @param levels  Sorted threshold vector (length N).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Integer class map (1-based, same shape as `I`).
/// @see multithresh
Value imquantize(const Value &I, const Value &levels,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Histogram matching to a reference image
/// (`[J, hgram] = imhistmatch(I, ref, nbins)`).
///
/// Adjusts `I`'s histogram to match `ref`'s via CDF matching
/// (single-channel), following MATLAB R2025b: `nbins` defaults to 64
/// for every class; both histograms use MATLAB's `imhist` centred
/// binning (`round(u·(nbins−1))`); each input bin maps to the output
/// grey level `(j−1)/(nbins−1)` where `j` is the smallest reference bin
/// with `cdfR(j) ≥ cdfI(i)`. Output class matches `I`.
///
/// @param I        Source image.
/// @param ref      Reference image (its histogram is the target).
/// @param nbins    Histogram bin count (≤ 0 → MATLAB default 64).
/// @param hgramOut Optional out-pointer; if non-null, receives `ref`'s
///                 histogram as a `1×nbins` double row (= `imhist(ref,
///                 nbins)'`, MATLAB's 2nd output).
/// @param mr       Memory resource (nullptr → process default).
/// @return         Matched image, same class as `I`.
/// @see histeq
Value imhistmatch(const Value &I, const Value &ref, int nbins,
                  Value *hgramOut = nullptr,
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

/// @brief Wavelet-display quantisation
/// (`Y = wcodemat(X, nb, opt, absol)`).
///
/// Quantises and scales `X` into integer codes in `[1, nb]`.
/// Output is DOUBLE; a canonical wavelet-display helper.
///
/// @param X      Input coefficient matrix.
/// @param nb     Code range upper bound.
/// @param opt    `"mat"` (default, global), `"row"`, or `"col"`.
/// @param absol  1 → use `abs(X)` (default), 0 → use `X`.
/// @param mr     Memory resource (nullptr → process default).
/// @return       DOUBLE matrix of integer codes in `[1, nb]`.
Value wcodemat(const Value &X, int nb, const std::string &opt, int absol,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Shannon entropy of the image histogram
/// (`H = entropy(I, nbins)`).
///
/// In bits. For non-logical images the image is first converted to
/// uint8 with 256 bins by default; logical images use 2 bins. Zero
/// counts are skipped before the log₂ sum.
///
/// @param I      Input image.
/// @param nbins  Histogram bin count.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scalar entropy in bits.
/// @see entropyfilt
Value entropy(const Value &I, int nbins,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Multilevel thresholding into an indexed image, level-count
/// form (`L = grayslice(I, N)`).
///
/// Equivalent to `grayslice(I, [(1/N) … ((N-1)/N)])` scaled to the
/// image's class range. Output is uint8 if `N < 256`, else `double`
/// (1-based indexing).
///
/// @param I   Input image.
/// @param N   Number of output levels (≥ 1).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Indexed image (uint8 or double).
/// @see imquantize, multithresh
Value grayslice(const Value &I, int N,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Multilevel thresholding into an indexed image, explicit
/// thresholds form (`L = grayslice(I, [t1, t2, …])`).
///
/// For floating-point images the threshold vector is clamped to
/// `[min(I), max(I)]` (extending toward image bounds, never
/// shrinking). Output is uint8 if `levels.size() < 256`, else
/// `double` (1-based indexing).
///
/// @param I       Input image.
/// @param levels  Explicit threshold values (any length).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Indexed image.
/// @see imquantize, multithresh
Value grayslice(const Value &I, Span<const double> levels,
                std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::image
