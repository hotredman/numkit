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
/// @param fs      Sample rate. Defaults to 2*pi (matches MATLAB convention
///                for normalised radian frequency). Affects PSD scaling
///                and the returned frequency vector range [0, fs/2].
std::tuple<Value, Value>
periodogram(const Value &x, const Value &window, size_t nfft, double fs = 2.0 * 3.14159265358979323846, std::pmr::memory_resource *mr = nullptr);

/// Welch's method: averaged, modified periodogram. Returns (Pxx, F).
///
/// @param window    Window vector. Empty → Hamming of length min(256, numel(x)).
/// @param noverlap  Samples of overlap between segments. 0 → winLen / 2.
/// @param nfft      FFT size. 0 → auto-pick nextPow2(winLen).
/// @param fs        Sample rate, default 2*pi (MATLAB convention).
std::tuple<Value, Value>
pwelch(const Value &x, const Value &window, size_t noverlap, size_t nfft, double fs = 2.0 * 3.14159265358979323846, std::pmr::memory_resource *mr = nullptr);

/// Cross-PSD via Welch's method. Returns (Pxy, F) where Pxy is the
/// one-sided complex cross-spectrum E[X(f)·Y*(f)]. Same windowing
/// and segmentation rules as pwelch.
std::tuple<Value, Value>
cpsd(const Value &x, const Value &y, const Value &window, size_t noverlap, size_t nfft, double fs = 2.0 * 3.14159265358979323846, std::pmr::memory_resource *mr = nullptr);

/// Magnitude-squared coherence via Welch's method. Returns (Cxy, F)
/// with Cxy(f) = |Pxy(f)|² / (Pxx(f)·Pyy(f)), real-valued in [0, 1].
std::tuple<Value, Value>
mscohere(const Value &x, const Value &y, const Value &window, size_t noverlap, size_t nfft, double fs = 2.0 * 3.14159265358979323846, std::pmr::memory_resource *mr = nullptr);

/// Transfer-function estimate Txy(f) = Pyx(f) / Pxx(f). For an LTI
/// system y = h * x this recovers H(f) up to finite-Welch bias.
/// Same windowing/segmentation parameters as pwelch.
std::tuple<Value, Value>
tfestimate(const Value &x, const Value &y, const Value &window, size_t noverlap, size_t nfft, double fs = 2.0 * 3.14159265358979323846, std::pmr::memory_resource *mr = nullptr);

/// Yule-Walker AR PSD of order p. Levinson-Durbin solves the
/// normal equations from the autocorrelation; returns the all-pole
/// PSD Pxx(f) = σ² / |1 + Σ a_k e^{-jωk}|² on a one-sided grid.
std::tuple<Value, Value>
pyulear(const Value &x, int p, size_t nfft, std::pmr::memory_resource *mr = nullptr);

/// Burg's AR PSD of order p. AR coefficients estimated by minimising
/// the sum of forward + backward prediction-error variances
/// iteratively — more numerically stable on short data than
/// Yule-Walker. Same output convention as pyulear.
std::tuple<Value, Value>
pburg(const Value &x, int p, size_t nfft, std::pmr::memory_resource *mr = nullptr);

// `aryule` and `lpc` live in libs/signal's parametric/signal_modeling
// stack — they predate this TU.

} // namespace numkit::signal
