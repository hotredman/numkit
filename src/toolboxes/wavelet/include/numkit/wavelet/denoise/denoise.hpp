/// @file denoise.hpp
/// @ingroup group_wavelet
// toolboxes/wavelet/include/numkit/wavelet/denoise/denoise.hpp
//
// Wavelet-domain denoising: thresholding rules (wthresh), noise σ
// estimation (wnoisest), and the composite VisuShrink-style
// denoiser (wdenoise).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::wavelet {

/// @addtogroup group_wavelet
/// @{


/// Element-wise wavelet coefficient thresholding (`Y = wthresh(X, sorh, T)`).
///
/// Applies one of two thresholding rules at level `T`:
///   - `sorh = "h"` (hard): @f$ Y = X\,\cdot\,\mathbb{1}(|X| > T) @f$
///     — zero out coefficients below the threshold, leave the rest
///     unchanged.
///   - `sorh = "s"` (soft): @f$ Y = \text{sign}(X)\,\max(|X|-T,\,0) @f$
///     — shrink every coefficient toward zero by `T`.
///
/// Shape of X is preserved; output type is always DOUBLE.
///
/// @param X     Input coefficients (any numeric Value).
/// @param sorh  `"h"` or `"s"`.
/// @param T     Non-negative threshold.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Thresholded coefficients.
/// @throws      Error if `sorh` is not `"h"`/`"H"`/`"s"`/`"S"`.
///
/// @see wnoisest, wdenoise
Value wthresh(const Value &X, const std::string &sorh, double T,
              std::pmr::memory_resource *mr = nullptr);

/// Per-level noise σ estimate (`σ = wnoisest(C, L, S)`).
///
/// Uses the median-absolute-deviation rule on each requested detail
/// band:
/// @f[ \hat\sigma_k = \text{median}(|cD_k|) / 0.6745 @f]
/// This is the standard robust estimator.
///
/// @param C   Coefficient row from @ref wavedec.
/// @param L   Bookkeeping row.
/// @param S   Row of 1-based detail levels (1 = finest).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Row of σ̂ estimates, one per element of `S`.
///
/// @see wthresh, wdenoise
Value wnoisest(const Value &C, const Value &L, const Value &S,
               std::pmr::memory_resource *mr = nullptr);

/// VisuShrink-style soft-threshold denoising (`y = wdenoise(x, level, wname)`).
///
/// Pipeline:
///   1. `[C, L] = wavedec(x, level, wname)` — multi-level DWT.
///   2. @f$ \hat\sigma = \text{median}(|cD_1|) / 0.6745 @f$ on the
///      finest detail band.
///   3. Universal (VisuShrink) threshold
///      @f$ T = \hat\sigma\,\sqrt{2\ln N} @f$ where N = length(x).
///   4. Soft-threshold every detail band in-place; leave cA untouched.
///   5. `y = waverec(C', L, wname)` — reconstruct.
///
/// Defaults: `level = min(floor(log2(N)), 5)`, `wname = "sym4"`. Pass
/// `level ≤ 0` or empty `wname` to request the default.
///
/// @param x      Noisy input signal.
/// @param level  Decomposition depth (≤ 0 → default).
/// @param wname  Wavelet name (empty → "sym4").
/// @param mr     Memory resource (nullptr → process default).
/// @return       Denoised signal of the same shape as `x`.
///
/// @code
/// auto y = wdenoise(noisy, 0, "");      // defaults: 5 levels, sym4
/// @endcode
///
/// @see wthresh, wnoisest, wavedec
Value wdenoise(const Value &x, int level, const std::string &wname,
               std::pmr::memory_resource *mr = nullptr);

/// Entropy of a coefficient vector (`E = wentropy(X, T[, P])`).
///
/// Closed-form additive entropy over the elements of `X` (a "cost" used by
/// wavelet-packet best-tree selection):
/// - `"shannon"`: `−Σ sᵢ²·log(sᵢ²)` (terms with `sᵢ=0` contribute 0).
/// - `"log energy"`: `Σ log(sᵢ²)` (nonzero `sᵢ` only).
/// - `"threshold"` (P = threshold): `#{i : |sᵢ| > P}`.
/// - `"sure"` (P = threshold): `n − 2·#{i : |sᵢ| ≤ P} + Σ min(sᵢ², P²)`.
/// - `"norm"` (P = exponent ≥ 1): `Σ |sᵢ|ᴾ`.
///
/// @param X      Coefficient vector (any shape; flattened).
/// @param type   Entropy type (case-insensitive).
/// @param param  `P` for threshold / sure / norm (ignored otherwise).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scalar entropy.
/// @throws       Error on an unknown type or `norm` exponent `< 1`.
/// @see wthresh, wdenoise
Value wentropy(const Value &X, const std::string &type, double param = 0.0,
               std::pmr::memory_resource *mr = nullptr);

/// Default denoising / compression parameters (`[thr, sorh, keepapp] =
/// ddencmp(opt, type, x)`).
struct DdencmpResult {
    Value thr;       ///< Threshold.
    Value sorh;      ///< Soft (`'s'`) or hard (`'h'`) thresholding.
    Value keepapp;   ///< Keep approximation flag (always 1).
};

/// Default threshold / settings for wavelet denoising or compression.
///
/// Estimates the noise level from the finest-detail coefficients of a
/// 1-level `db1` DWT — `σ̂ = median(|cD₁|)/0.6745` — and returns the default
/// parameters for `opt = "den"` (denoising) or `opt = "cmp"` (compression):
/// - **den**: `thr = sqrt(2·log(n))·σ̂` (universal threshold), `sorh = 's'`.
/// - **cmp**: `thr = median(|cD₁|)`, `sorh = 'h'`.
///
/// `keepapp = 1` in both cases. Only `type = "wv"` (wavelet) is supported;
/// `"wp"` (wavelet packet) is not yet implemented.
///
/// @param opt   `"den"` (denoise) or `"cmp"` (compress).
/// @param type  `"wv"` (wavelet); `"wp"` is rejected.
/// @param x     Input signal.
/// @param mr    Memory resource (nullptr → process default).
/// @return      `{thr, sorh, keepapp}` (see @ref DdencmpResult).
/// @throws      Error on an unknown `opt`/`type` or `"wp"`.
/// @see wentropy, wthresh, wdenoise
DdencmpResult ddencmp(const std::string &opt, const std::string &type,
                      const Value &x, std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::wavelet
