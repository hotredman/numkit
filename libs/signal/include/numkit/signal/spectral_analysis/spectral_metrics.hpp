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
Value bandpower(std::pmr::memory_resource *mr, const Value &x,
                const Value *fs = nullptr, const Value *freqrange = nullptr);

/// meanfreq(x[, fs]) — power-weighted mean frequency:
/// sum(f * P) / sum(P).
Value meanfreq(std::pmr::memory_resource *mr, const Value &x,
               const Value *fs = nullptr);

/// medfreq(x[, fs]) — median frequency: F such that the cumulative
/// power below F equals half the total power.
Value medfreq(std::pmr::memory_resource *mr, const Value &x,
              const Value *fs = nullptr);

/// enbw(window[, fs]) — equivalent noise bandwidth of a window:
/// fs * sum(w.^2) / sum(w).^2  (returns dimensionless if fs absent).
Value enbw(std::pmr::memory_resource *mr, const Value &window,
           const Value *fs = nullptr);

/// obw(x[, fs[, p]]) — occupied-bandwidth at fraction p of total power
/// (default p = 0.99). Returns the bandwidth (Hz or rad/s).
Value obw(std::pmr::memory_resource *mr, const Value &x,
          const Value *fs = nullptr, double p = 0.99);

/// powerbw(x[, fs]) — 3-dB (half-peak) power bandwidth around the
/// dominant peak.
Value powerbw(std::pmr::memory_resource *mr, const Value &x,
              const Value *fs = nullptr);

/// spectralcrest(x[, fs]) — max(P) / mean(P). Peakedness ratio.
Value spectralcrest(std::pmr::memory_resource *mr, const Value &x,
                    const Value *fs = nullptr);

/// spectralflatness(x[, fs]) — geomean(P) / mean(P). 1 = white, 0 = tonal.
Value spectralflatness(std::pmr::memory_resource *mr, const Value &x,
                       const Value *fs = nullptr);

/// spectralentropy(x[, fs]) — Shannon entropy of the normalised PSD.
Value spectralentropy(std::pmr::memory_resource *mr, const Value &x,
                      const Value *fs = nullptr);

/// spectralkurtosis(x[, fs]) — kurtosis of the PSD, treated as a
/// distribution over frequency.
Value spectralkurtosis(std::pmr::memory_resource *mr, const Value &x,
                       const Value *fs = nullptr);

/// spectralskewness(x[, fs]) — skewness of the PSD as a frequency distribution.
Value spectralskewness(std::pmr::memory_resource *mr, const Value &x,
                       const Value *fs = nullptr);

} // namespace numkit::signal
