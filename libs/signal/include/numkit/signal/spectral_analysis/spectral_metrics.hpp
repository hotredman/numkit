// libs/signal/include/numkit/signal/spectral_analysis/spectral_metrics.hpp
//
// Spectral measurement functions.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// @file
/// @brief Spectral measurement functions.
///
/// **Conventions:**
/// - `fs` defaults to `Value::Empty`. With `fs.isEmpty()` frequencies
///   span `[0, π]` rad/sample; with `fs > 0` they span `[0, fs/2]` Hz.
/// - Functions compute the PSD internally via a single-segment
///   periodogram.

/// @brief Power in a frequency band (`p = bandpower(x, fs, freqrange)`).
///
/// - `fs.isEmpty()`: returns `mean(x^2)` (Parseval-equivalent total
///   power, ignoring `freqrange`).
/// - `fs > 0` and `freqrange.isEmpty()`: integrates the full PSD.
/// - `fs > 0` and `freqrange = [f1 f2]`: integrates the PSD over
///   `[f1, f2]`.
///
/// @param x          Real 1-D signal.
/// @param fs         Sample rate in Hz, or `Value::Empty`.
/// @param freqrange  Optional `[fLo, fHi]` band edges in Hz.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Scalar DOUBLE power.
Value bandpower(const Value &x,
                const Value &fs = Value::Empty,
                const Value &freqrange = Value::Empty,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Power-weighted mean frequency.
///
/// @f$ \dfrac{\sum f \cdot P}{\sum P} @f$.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty` for rad/sample.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar mean frequency.
/// @see medfreq, powerbw
Value meanfreq(const Value &x, const Value &fs = Value::Empty,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Median frequency.
///
/// `F` such that the cumulative PSD below `F` equals half the total.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar median frequency.
/// @see meanfreq
Value medfreq(const Value &x, const Value &fs = Value::Empty,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Equivalent noise bandwidth of a window
/// (`b = enbw(window, fs)`).
///
/// @f$ \mathrm{ENBW} = f_s \cdot \sum w_n^2 \big/ (\sum w_n)^2 @f$.
/// Dimensionless when `fs.isEmpty()`.
///
/// @param window  Window vector.
/// @param fs      Sample rate, or `Value::Empty`.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Scalar ENBW.
Value enbw(const Value &window, const Value &fs = Value::Empty,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Occupied bandwidth at fraction `p` of total power
/// (`bw = obw(x, fs, p)`).
///
/// Bandwidth (Hz or rad/s) of the smallest contiguous frequency
/// interval containing at least fraction `p` of total power.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate or `Value::Empty`.
/// @param p   Power fraction (default 0.99).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar bandwidth.
Value obw(const Value &x, const Value &fs = Value::Empty,
          double p = 0.99,
          std::pmr::memory_resource *mr = nullptr);

/// @brief 3-dB power bandwidth around the dominant peak.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar bandwidth in Hz / rad/s.
/// @see obw, meanfreq
Value powerbw(const Value &x, const Value &fs = Value::Empty,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral crest: `max(P) / mean(P)` (peakedness).
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar crest.
/// @see spectralflatness
Value spectralcrest(const Value &x, const Value &fs = Value::Empty,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral flatness: `geomean(P) / mean(P)`.
///
/// `1` = white noise, `0` = tonal.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar flatness in `[0, 1]`.
/// @see spectralcrest
Value spectralflatness(const Value &x, const Value &fs = Value::Empty,
                       std::pmr::memory_resource *mr = nullptr);

/// @brief Shannon entropy of the normalised PSD.
///
/// Treats the PSD as a probability distribution over frequency bins.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar entropy.
Value spectralentropy(const Value &x, const Value &fs = Value::Empty,
                      std::pmr::memory_resource *mr = nullptr);

/// @brief Kurtosis of the PSD treated as a frequency distribution.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar spectral kurtosis.
/// @see spectralskewness
Value spectralkurtosis(const Value &x, const Value &fs = Value::Empty,
                       std::pmr::memory_resource *mr = nullptr);

/// @brief Skewness of the PSD treated as a frequency distribution.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar spectral skewness.
/// @see spectralkurtosis
Value spectralskewness(const Value &x, const Value &fs = Value::Empty,
                       std::pmr::memory_resource *mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// Harmonic / SNR family
//
// All compute fundamental power and noise / harmonic power on the
// periodogram, then return the ratio in dB. Default behaviour treats
// the dominant peak (excluding DC) as the fundamental and uses N = 6
// harmonics.
// ─────────────────────────────────────────────────────────────────────

/// @brief Signal-to-noise ratio in dB.
///
/// Power of the fundamental over the noise floor (everything outside
/// the fundamental and the first 6 harmonic skirts).
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar SNR in dB.
/// @see sinad, thd, sfdr
Value snr(const Value &x, const Value &fs = Value::Empty,
          std::pmr::memory_resource *mr = nullptr);

/// @brief Signal-to-noise-and-distortion in dB.
///
/// Like @ref snr but treats harmonic distortion as noise.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar SINAD in dB.
/// @see snr, thd
Value sinad(const Value &x, const Value &fs = Value::Empty,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Total harmonic distortion in dB.
///
/// Power sum of the first 6 harmonics over the fundamental.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar THD in dB.
/// @see snr, sinad
Value thd(const Value &x, const Value &fs = Value::Empty,
          std::pmr::memory_resource *mr = nullptr);

/// @brief Spurious-free dynamic range in dB.
///
/// Distance from the fundamental peak to the next-largest spur.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Scalar SFDR in dB.
/// @see snr
Value sfdr(const Value &x, const Value &fs = Value::Empty,
           std::pmr::memory_resource *mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// Analytic-signal-based instantaneous metrics
// ─────────────────────────────────────────────────────────────────────

/// @brief Instantaneous frequency via Hilbert transform.
///
/// @f$ f_i[n] = \dfrac{1}{2\pi}\,\dfrac{d\,\mathrm{unwrap}(\angle z[n])}{dt} @f$
/// where `z = hilbert(x)`. Implemented as centered differences of the
/// unwrapped analytic-signal phase.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty` (output in rad/sample).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of length `numel(x) - 1`.
/// @see instbw, hilbert
Value instfreq(const Value &x, const Value &fs = Value::Empty,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Instantaneous bandwidth.
///
/// @f$ b_i = |dA/dt| \big/ (2\pi A) @f$
/// where `A` is the analytic-signal envelope.
///
/// @param x   Real 1-D signal.
/// @param fs  Sample rate (Hz) or `Value::Empty`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of length `numel(x) - 1`.
/// @see instfreq, hilbert
Value instbw(const Value &x, const Value &fs = Value::Empty,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
