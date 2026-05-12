// libs/audio/include/numkit/audio/spectral/shape_descriptors.hpp
//
// MATLAB Audio Toolbox spectral shape descriptors.
//
// Every function in this header accepts EITHER of two argument forms:
//   1. `(x, fs)` — `x` is a column-vector time-domain signal, `fs` is
//      the sample rate. Internally computes STFT with
//      `rectwin(round(fs·0.03))`, overlap `round(fs·0.02)`,
//      `FFTLength = winLen`. Returns a per-frame column vector.
//   2. `(X, F)` — `X` is a power (or magnitude) spectrum, `F` is the
//      frequency vector. `X` may be `M × N` (one spectrum per column).
//      Returns a per-column metric as a row vector.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::audio {

/// Spectral centroid — power-weighted mean frequency of each spectrum.
///
/// \f$ \mu_X = \sum_k f_k \cdot X_k \big/ \sum_k X_k \f$
///
/// @param x   Time signal (with fs in `f`) or `M × N` spectrum.
/// @param f   Sample rate (scalar) or frequency vector (`M`-length).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Per-frame / per-column centroid.
///
/// @see spectralSpread
Value spectralCentroid(const Value &                x,
                       const Value &                f,
                       std::pmr::memory_resource *  mr = nullptr);

/// Spectral spread — power-weighted standard deviation around the centroid.
///
/// @copydoc spectralCentroid
/// @see spectralCentroid, spectralSkewness, spectralKurtosis
Value spectralSpread(const Value &                x,
                     const Value &                f,
                     std::pmr::memory_resource *  mr = nullptr);

/// Spectral rolloff frequency — `f` such that the cumulative spectral
/// power below `f` equals `percentile` of the total power.
///
/// @param x           Time signal or spectrum (see header note).
/// @param f           Sample rate or frequency vector.
/// @param percentile  Cumulative power fraction in (0, 1). Default 0.95.
/// @param mr          Memory resource (nullptr → process default).
/// @return            Per-frame / per-column rolloff frequency.
Value spectralRolloffPoint(const Value &                x,
                           const Value &                f,
                           double                       percentile = 0.95,
                           std::pmr::memory_resource *  mr         = nullptr);

/// Spectral decrease — slope-like measure based on spectrum shape relative
/// to its first bin.
///
/// \f$ d = \frac{1}{\sum_{k \ge 2} X_k} \sum_{k \ge 2} \frac{X_k - X_1}{k - 1} \f$
///
/// @copydoc spectralCentroid
Value spectralDecrease(const Value &                x,
                       const Value &                f,
                       std::pmr::memory_resource *  mr = nullptr);

/// Spectral slope — least-squares slope of `(f, X)` per frame / column.
/// @copydoc spectralCentroid
Value spectralSlope(const Value &                x,
                    const Value &                f,
                    std::pmr::memory_resource *  mr = nullptr);

/// Spectral flux — `p`-norm of frame-to-frame spectrum differences.
///
/// @param x   Time signal or spectrum.
/// @param f   Sample rate or frequency vector.
/// @param p   Norm order (default 2 — Euclidean).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `N - 1` flux values (one per frame transition).
Value spectralFlux(const Value &                x,
                   const Value &                f,
                   double                       p  = 2.0,
                   std::pmr::memory_resource *  mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// Cycle I: shape statistics. All match MATLAB R2025b Signal Toolbox.
// Frequency moments (Kurtosis / Skewness) use the X-weighted
// central-moment formula from MATLAB's `spectralSkewness.m`.
// ─────────────────────────────────────────────────────────────────────

/// Spectral crest — `max(X) / mean(X)` per frame.
/// @copydoc spectralCentroid
/// @see spectralFlatness
Value spectralCrest(const Value &                x,
                    const Value &                f,
                    std::pmr::memory_resource *  mr = nullptr);

/// Spectral entropy — Shannon entropy of the normalised spectrum.
/// @copydoc spectralCentroid
Value spectralEntropy(const Value &                x,
                      const Value &                f,
                      std::pmr::memory_resource *  mr = nullptr);

/// Spectral flatness — `geomean(X) / mean(X)`. 1 = white, 0 = tonal.
/// @copydoc spectralCentroid
/// @see spectralCrest
Value spectralFlatness(const Value &                x,
                       const Value &                f,
                       std::pmr::memory_resource *  mr = nullptr);

/// Spectral kurtosis — X-weighted 4th central moment.
/// @copydoc spectralCentroid
/// @see spectralSkewness, spectralSpread
Value spectralKurtosis(const Value &                x,
                       const Value &                f,
                       std::pmr::memory_resource *  mr = nullptr);

/// Spectral skewness — X-weighted 3rd central moment.
/// @copydoc spectralCentroid
/// @see spectralKurtosis, spectralSpread
Value spectralSkewness(const Value &                x,
                       const Value &                f,
                       std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::audio
