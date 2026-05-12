// libs/signal/include/numkit/signal/spectral_analysis/spectral_metrics.hpp
//
// Spectral measurement functions (MATLAB Signal Processing Toolbox):
//   bandpower / meanfreq / medfreq / enbw / obw / powerbw
//   spectralcrest / spectralflatness / spectralentropy
//   spectralkurtosis / spectralskewness
//   snr / sinad / thd / sfdr (peak-finding harmonic measurements)
//   instfreq / instbw (analytic-signal-based)
//
// Conventions:
//   * `fs` defaults to 2π (so frequencies span [0, π] when omitted —
//     matches the unitless MATLAB default with no fs supplied).
//   * Functions accept either a time-series `x` (then PSD computed
//     internally via single-segment periodogram) OR a 2-arg form
//     (Pxx, F) with the user's PSD already in hand.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// bandpower(x[, fs[, freqrange]]) — total power in `freqrange` (or
/// full band) of x. With no fs, returns mean(x.^2). With fs but no
/// freqrange, integrates the full PSD. With freqrange = [f1 f2],
/// integrates the PSD over [f1, f2].
Value bandpower(const Value &x, const Value *fs = nullptr, const Value *freqrange = nullptr, std::pmr::memory_resource *mr = nullptr);

/// meanfreq(x[, fs]) — power-weighted mean frequency:
/// sum(f * P) / sum(P).
Value meanfreq(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// medfreq(x[, fs]) — median frequency: F such that the cumulative
/// power below F equals half the total power.
Value medfreq(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// enbw(window[, fs]) — equivalent noise bandwidth of a window:
/// fs * sum(w.^2) / sum(w).^2  (returns dimensionless if fs absent).
Value enbw(const Value &window, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// obw(x[, fs[, p]]) — occupied-bandwidth at fraction p of total power
/// (default p = 0.99). Returns the bandwidth (Hz or rad/s).
Value obw(const Value &x, const Value *fs = nullptr, double p = 0.99, std::pmr::memory_resource *mr = nullptr);

/// powerbw(x[, fs]) — 3-dB (half-peak) power bandwidth around the
/// dominant peak.
Value powerbw(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// spectralcrest(x[, fs]) — max(P) / mean(P). Peakedness ratio.
Value spectralcrest(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// spectralflatness(x[, fs]) — geomean(P) / mean(P). 1 = white, 0 = tonal.
Value spectralflatness(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// spectralentropy(x[, fs]) — Shannon entropy of the normalised PSD.
Value spectralentropy(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// spectralkurtosis(x[, fs]) — kurtosis of the PSD, treated as a
/// distribution over frequency.
Value spectralkurtosis(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// spectralskewness(x[, fs]) — skewness of the PSD as a frequency distribution.
Value spectralskewness(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

// ── Harmonic / SNR family ──────────────────────────────────────────
//
// All compute fundamental power and noise / harmonic power on the
// periodogram, then return the ratio in dB. Default behaviour treats
// the dominant peak (excluding DC) as the fundamental and N=6 harmonics.

/// snr(x[, fs]) — signal-to-noise in dB. Power of the fundamental
/// over the noise floor (everything outside the fundamental and the
/// first 6 harmonic skirts).
Value snr(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// sinad(x[, fs]) — signal-to-noise-and-distortion in dB. Like snr
/// but treats harmonics as noise (denominator includes them).
Value sinad(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// thd(x[, fs]) — total harmonic distortion in dB. Power sum of the
/// first 6 harmonics over the fundamental.
Value thd(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// sfdr(x[, fs]) — spurious-free dynamic range in dB. Distance from
/// the fundamental peak to the next-largest spur (any local maximum
/// outside the fundamental's skirt).
Value sfdr(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

// ── Analytic-signal-based instantaneous metrics ────────────────────

/// instfreq(x[, fs]) — instantaneous frequency via Hilbert transform.
/// Returns a column vector of length numel(x)-1 (centred-difference of
/// the unwrapped phase). With fs >0, units are Hz; otherwise rad/sample.
Value instfreq(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

/// instbw(x[, fs]) — instantaneous bandwidth: |dA/dt| / (2π·A) where
/// A is the analytic-signal envelope. Column vector of length
/// numel(x)-1.
Value instbw(const Value &x, const Value *fs = nullptr, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
