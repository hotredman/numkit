// libs/signal/include/numkit/signal/spectral_analysis/periodogram_pwelch.hpp
//
// Power spectrum estimation: periodogram + Welch's method.
// spectrogram (the time-frequency variant of pwelch that retains every
// segment) lives in time_frequency/spectrogram.hpp.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Single-segment periodogram PSD estimate.
///
/// Computes
/// \f$ \hat{P}_{xx}(f) = \frac{1}{f_s \cdot \sum |w|^2} \big|\sum_n w[n] \, x[n] \, e^{-j 2\pi f n / f_s}\big|^2 \f$
/// on a uniform grid of frequencies on `[0, fs/2]` (one-sided form).
///
/// @param x       Real 1-D signal.
/// @param window  Window vector (must have `numel(window) == numel(x)`).
///                `Value::Empty` → rectangular window (all ones).
/// @param nfft    FFT size. `0` → `nextPow2(numel(x))`.
/// @param fs      Sample rate in Hz. Default `2π` (matches MATLAB's
///                normalised-radian convention).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(Pxx, F)`:
///                  * `Pxx` — `nfft/2+1 × 1` PSD column vector.
///                  * `F`   — `nfft/2+1 × 1` frequency vector on `[0, fs/2]`.
///
/// @see pwelch, spectrogram
std::tuple<Value, Value>
periodogram(const Value &                x,
            const Value &                window,
            size_t                       nfft,
            double                       fs = 2.0 * 3.14159265358979323846,
            std::pmr::memory_resource *  mr = nullptr);

/// Welch's averaged, modified periodogram PSD estimate.
///
/// Slides a window over `x` with `noverlap` samples of overlap,
/// periodogram-estimates each segment, then averages.
///
/// @param x          Real 1-D signal.
/// @param window     Window vector. `Value::Empty` → Hamming of length
///                   `min(256, numel(x))`.
/// @param noverlap   Samples of overlap. `0` → `numel(window)/2`.
/// @param nfft       FFT size. `0` → `nextPow2(numel(window))`.
/// @param fs         Sample rate. Default `2π`.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Tuple `(Pxx, F)` (see `periodogram`).
///
/// @see periodogram, cpsd, mscohere
std::tuple<Value, Value>
pwelch(const Value &                x,
       const Value &                window,
       size_t                       noverlap,
       size_t                       nfft,
       double                       fs = 2.0 * 3.14159265358979323846,
       std::pmr::memory_resource *  mr = nullptr);

/// Cross-PSD via Welch's method.
///
/// Estimates the one-sided cross-spectrum
/// \f$ P_{xy}(f) = E[X(f) \cdot Y^*(f)] \f$ using the same windowing /
/// segmentation rules as `pwelch`.
///
/// @param x          First real 1-D signal.
/// @param y          Second real 1-D signal (same length as `x`).
/// @param window     Window. `Value::Empty` → Hamming(256).
/// @param noverlap   Overlap. `0` → half window length.
/// @param nfft       FFT size. `0` → nextPow2(winLen).
/// @param fs         Sample rate.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Tuple `(Pxy, F)` — Pxy is COMPLEX.
///
/// @see pwelch, mscohere, tfestimate
std::tuple<Value, Value>
cpsd(const Value &                x,
     const Value &                y,
     const Value &                window,
     size_t                       noverlap,
     size_t                       nfft,
     double                       fs = 2.0 * 3.14159265358979323846,
     std::pmr::memory_resource *  mr = nullptr);

/// Magnitude-squared coherence via Welch's method.
///
/// Computes
/// \f$ C_{xy}(f) = \frac{|P_{xy}(f)|^2}{P_{xx}(f) \cdot P_{yy}(f)} \f$,
/// real-valued in `[0, 1]`. Equals 1 when y is a linear function of x.
///
/// @param x          First real 1-D signal.
/// @param y          Second real 1-D signal.
/// @param window     Window. `Value::Empty` → Hamming(256).
/// @param noverlap   Overlap.
/// @param nfft       FFT size.
/// @param fs         Sample rate.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Tuple `(Cxy, F)` — Cxy is REAL in `[0, 1]`.
///
/// @see cpsd, tfestimate
std::tuple<Value, Value>
mscohere(const Value &                x,
         const Value &                y,
         const Value &                window,
         size_t                       noverlap,
         size_t                       nfft,
         double                       fs = 2.0 * 3.14159265358979323846,
         std::pmr::memory_resource *  mr = nullptr);

/// Transfer-function estimate via Welch's method.
///
/// Estimates \f$ T_{xy}(f) = P_{yx}(f) / P_{xx}(f) \f$. For an LTI
/// system `y = h * x` this recovers `H(f)` up to finite-Welch bias.
///
/// @param x          Input signal.
/// @param y          Output signal.
/// @param window     Window. `Value::Empty` → Hamming(256).
/// @param noverlap   Overlap.
/// @param nfft       FFT size.
/// @param fs         Sample rate.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Tuple `(Txy, F)` — Txy is COMPLEX.
///
/// @see cpsd, mscohere, freqz
std::tuple<Value, Value>
tfestimate(const Value &                x,
           const Value &                y,
           const Value &                window,
           size_t                       noverlap,
           size_t                       nfft,
           double                       fs = 2.0 * 3.14159265358979323846,
           std::pmr::memory_resource *  mr = nullptr);

/// Yule-Walker autoregressive PSD estimate.
///
/// Solves the normal equations from the biased autocorrelation via
/// Levinson-Durbin recursion, then evaluates the all-pole PSD
/// \f$ P_{xx}(f) = \sigma^2 \big/ \big|1 + \sum a_k e^{-j\omega k}\big|^2 \f$
/// on a one-sided grid.
///
/// @param x     Real 1-D signal.
/// @param p     AR model order.
/// @param nfft  Number of frequency points (≥ 2). `0` → `nextPow2(numel(x))`.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Tuple `(Pxx, F)`.
///
/// @see pburg, aryule
std::tuple<Value, Value>
pyulear(const Value &                x,
        int                          p,
        size_t                       nfft,
        std::pmr::memory_resource *  mr = nullptr);

/// Burg's AR PSD estimate.
///
/// AR coefficients estimated by minimising the sum of forward and
/// backward prediction-error variances iteratively. More numerically
/// stable than Yule-Walker on short data.
///
/// @copydoc pyulear
/// @see pyulear, arburg
std::tuple<Value, Value>
pburg(const Value &                x,
      int                          p,
      size_t                       nfft,
      std::pmr::memory_resource *  mr = nullptr);

// `aryule` and `lpc` live in libs/signal's signal_modeling header.

} // namespace numkit::signal
