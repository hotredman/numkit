// libs/signal/include/numkit/signal/spectral_analysis/spectral_metrics.hpp
//
// Spectral measurement functions (MATLAB Signal Processing Toolbox).
//
// Conventions:
//   * `fs` defaults to `Value::Empty`. With `fs.isEmpty()` frequencies
//     span [0, π] rad/sample; with `fs > 0` they span [0, fs/2] Hz.
//   * Functions compute the PSD internally via a single-segment
//     periodogram.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Power in a frequency band.
///
/// With `fs.isEmpty()`: returns `mean(x^2)` (Parseval-equivalent total
/// power, ignoring `freqrange`). With `fs > 0` and `freqrange.isEmpty()`:
/// integrates the full PSD. With `fs > 0` and `freqrange = [f1 f2]`:
/// integrates the PSD over `[f1, f2]`.
///
/// @param x          Real 1-D signal.
/// @param fs         Sample rate in Hz, or `Value::Empty` for time-domain
///                   total-power shortcut.
/// @param freqrange  Optional `[fLo, fHi]` band edges in Hz. `Value::Empty`
///                   → full PSD.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Scalar DOUBLE power.
Value bandpower(const Value &                x,
                const Value &                fs        = Value::Empty,
                const Value &                freqrange = Value::Empty,
                std::pmr::memory_resource *  mr        = nullptr);

/// Power-weighted mean frequency: \f$ \sum f \cdot P / \sum P \f$.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty` for rad/sample.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar mean frequency.
Value meanfreq(const Value &                x,
               const Value &                fs = Value::Empty,
               std::pmr::memory_resource *  mr = nullptr);

/// Median frequency: F such that cumulative PSD below F equals half total.
/// @copydoc meanfreq
Value medfreq(const Value &                x,
              const Value &                fs = Value::Empty,
              std::pmr::memory_resource *  mr = nullptr);

/// Equivalent noise bandwidth of a window.
///
/// Computes \f$ \mathrm{ENBW} = f_s \cdot \sum w_n^2 / (\sum w_n)^2 \f$.
/// Dimensionless when `fs.isEmpty()`.
///
/// @param window  Window vector.
/// @param fs      Sample rate, or `Value::Empty`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Scalar ENBW.
Value enbw(const Value &                window,
           const Value &                fs = Value::Empty,
           std::pmr::memory_resource *  mr = nullptr);

/// Occupied bandwidth at fraction `p` of total power.
///
/// Returns the bandwidth (Hz or rad/s) of the smallest contiguous
/// frequency interval containing at least fraction `p` of total power.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate or `Value::Empty`.
/// @param p   Power fraction. Default 0.99.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar bandwidth.
Value obw(const Value &                x,
          const Value &                fs = Value::Empty,
          double                       p  = 0.99,
          std::pmr::memory_resource *  mr = nullptr);

/// 3-dB power bandwidth around the dominant peak.
/// @copydoc meanfreq
Value powerbw(const Value &                x,
              const Value &                fs = Value::Empty,
              std::pmr::memory_resource *  mr = nullptr);

/// Spectral crest: `max(P) / mean(P)`. Measures peakedness.
/// @copydoc meanfreq
Value spectralcrest(const Value &                x,
                    const Value &                fs = Value::Empty,
                    std::pmr::memory_resource *  mr = nullptr);

/// Spectral flatness: `geomean(P) / mean(P)`. 1 = white noise, 0 = tonal.
/// @copydoc meanfreq
Value spectralflatness(const Value &                x,
                       const Value &                fs = Value::Empty,
                       std::pmr::memory_resource *  mr = nullptr);

/// Shannon entropy of the normalised PSD (treated as a probability
/// distribution over frequency bins).
/// @copydoc meanfreq
Value spectralentropy(const Value &                x,
                      const Value &                fs = Value::Empty,
                      std::pmr::memory_resource *  mr = nullptr);

/// Kurtosis of the PSD treated as a frequency distribution.
/// @copydoc meanfreq
Value spectralkurtosis(const Value &                x,
                       const Value &                fs = Value::Empty,
                       std::pmr::memory_resource *  mr = nullptr);

/// Skewness of the PSD treated as a frequency distribution.
/// @copydoc meanfreq
Value spectralskewness(const Value &                x,
                       const Value &                fs = Value::Empty,
                       std::pmr::memory_resource *  mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// Harmonic / SNR family
//
// All compute fundamental power and noise / harmonic power on the
// periodogram, then return the ratio in dB. Default behaviour treats
// the dominant peak (excluding DC) as the fundamental and uses N = 6
// harmonics.
// ─────────────────────────────────────────────────────────────────────

/// Signal-to-noise ratio in dB.
///
/// Power of the fundamental over the noise floor (everything outside
/// the fundamental and the first 6 harmonic skirts).
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar SNR in dB.
///
/// @see sinad, thd, sfdr
Value snr(const Value &                x,
          const Value &                fs = Value::Empty,
          std::pmr::memory_resource *  mr = nullptr);

/// Signal-to-noise-and-distortion in dB.
/// Like `snr` but treats harmonic distortion as noise.
/// @copydoc snr
Value sinad(const Value &                x,
            const Value &                fs = Value::Empty,
            std::pmr::memory_resource *  mr = nullptr);

/// Total harmonic distortion in dB.
/// Power sum of the first 6 harmonics over the fundamental.
/// @copydoc snr
Value thd(const Value &                x,
          const Value &                fs = Value::Empty,
          std::pmr::memory_resource *  mr = nullptr);

/// Spurious-free dynamic range in dB.
/// Distance from the fundamental peak to the next-largest spur.
/// @copydoc snr
Value sfdr(const Value &                x,
           const Value &                fs = Value::Empty,
           std::pmr::memory_resource *  mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// Analytic-signal-based instantaneous metrics
// ─────────────────────────────────────────────────────────────────────

/// Instantaneous frequency via Hilbert transform.
///
/// Computes \f$ f_i[n] = \frac{1}{2\pi} \frac{d\,\mathrm{unwrap}(\angle z[n])}{dt} \f$
/// where `z = hilbert(x)`. Implemented as centered differences of the
/// unwrapped analytic-signal phase.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty` (output in rad/sample).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of length `numel(x) - 1`.
///
/// @see instbw, hilbert
Value instfreq(const Value &                x,
               const Value &                fs = Value::Empty,
               std::pmr::memory_resource *  mr = nullptr);

/// Instantaneous bandwidth: \f$ b_i = |dA/dt| / (2\pi A) \f$
/// where `A` is the analytic-signal envelope.
///
/// @copydoc instfreq
/// @see instfreq, hilbert
Value instbw(const Value &                x,
             const Value &                fs = Value::Empty,
             std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
