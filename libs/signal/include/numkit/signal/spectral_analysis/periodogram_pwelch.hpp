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

/// Single-segment periodogram power spectral density estimate.
/// Returns (Pxx, F).
///
/// @param window  Optional window vector of length numel(x). Pass an empty
///                Value to use a rectangular window (all ones).
/// @param nfft    FFT size. Pass 0 to auto-pick nextPow2(numel(x)).
std::tuple<Value, Value>
periodogram(std::pmr::memory_resource *mr, const Value &x, const Value &window, size_t nfft);

/// Welch's method: averaged, modified periodogram. Returns (Pxx, F).
///
/// @param window    Window vector. Empty → Hamming of length min(256, numel(x)).
/// @param noverlap  Samples of overlap between segments. 0 → winLen / 2.
/// @param nfft      FFT size. 0 → auto-pick nextPow2(winLen).
std::tuple<Value, Value>
pwelch(std::pmr::memory_resource *mr,
       const Value &x,
       const Value &window,
       size_t noverlap,
       size_t nfft);

/// Cross-PSD via Welch's method. Returns (Pxy, F) where Pxy is the
/// one-sided complex cross-spectrum E[X(f)·Y*(f)]. Same windowing
/// and segmentation rules as pwelch.
std::tuple<Value, Value>
cpsd(std::pmr::memory_resource *mr,
     const Value &x, const Value &y,
     const Value &window, size_t noverlap, size_t nfft);

/// Magnitude-squared coherence via Welch's method. Returns (Cxy, F)
/// with Cxy(f) = |Pxy(f)|² / (Pxx(f)·Pyy(f)), real-valued in [0, 1].
std::tuple<Value, Value>
mscohere(std::pmr::memory_resource *mr,
         const Value &x, const Value &y,
         const Value &window, size_t noverlap, size_t nfft);

/// Transfer-function estimate Txy(f) = Pyx(f) / Pxx(f). For an LTI
/// system y = h * x this recovers H(f) up to finite-Welch bias.
/// Same windowing/segmentation parameters as pwelch.
std::tuple<Value, Value>
tfestimate(std::pmr::memory_resource *mr,
           const Value &x, const Value &y,
           const Value &window, size_t noverlap, size_t nfft);

/// Yule-Walker AR PSD of order p. Levinson-Durbin solves the
/// normal equations from the autocorrelation; returns the all-pole
/// PSD Pxx(f) = σ² / |1 + Σ a_k e^{-jωk}|² on a one-sided grid.
std::tuple<Value, Value>
pyulear(std::pmr::memory_resource *mr,
        const Value &x, int p, size_t nfft);

/// Burg's AR PSD of order p. AR coefficients estimated by minimising
/// the sum of forward + backward prediction-error variances
/// iteratively — more numerically stable on short data than
/// Yule-Walker. Same output convention as pyulear.
std::tuple<Value, Value>
pburg(std::pmr::memory_resource *mr,
      const Value &x, int p, size_t nfft);

} // namespace numkit::signal
