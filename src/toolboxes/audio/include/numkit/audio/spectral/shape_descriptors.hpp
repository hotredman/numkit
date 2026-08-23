// toolboxes/audio/include/numkit/audio/spectral/shape_descriptors.hpp
//
// Audio spectral shape descriptors.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::audio {

/// @file
/// @ingroup group_audio
/// @brief Spectral-shape descriptors.
///
/// **Two input forms** (shared by every function in this header):
/// 1. `(x, fs)` — `x` is a column-vector time-domain signal, `fs` is
///    the sample rate. Internally computes STFT with
///    `rectwin(round(fs · 0.03))`, overlap `round(fs · 0.02)`,
///    `FFTLength = winLen`. Returns a per-frame column vector.
/// 2. `(X, F)` — `X` is a power (or magnitude) spectrum, `F` is the
///    frequency vector. `X` may be `M × N` (one spectrum per column).
///    Returns a per-column metric as a row vector.
///
/// Throughout this header `x` / `f` follow that convention; only
/// descriptor-specific extra arguments (`percentile`, `p`) are called
/// out separately.

/// @brief Spectral centroid — power-weighted mean frequency.
///
/// @f$ \mu_X = \sum_k f_k \cdot X_k \big/ \sum_k X_k @f$
///
/// @param x   Time signal or `M × N` spectrum (see file note).
/// @param f   Sample rate or frequency vector (see file note).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Per-frame / per-column centroid.
/// @see spectralSpread
Value spectralCentroid(const Value &x, const Value &f,
                       std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral spread — power-weighted std around the centroid.
///
/// @param x   Time signal or spectrum.
/// @param f   Sample rate or frequency vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Per-frame / per-column spread.
/// @see spectralCentroid, spectralSkewness, spectralKurtosis
Value spectralSpread(const Value &x, const Value &f,
                     std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral rolloff frequency.
///
/// `f` such that the cumulative spectral power below `f` equals
/// `percentile` of the total power.
///
/// @param x           Time signal or spectrum.
/// @param f           Sample rate or frequency vector.
/// @param percentile  Cumulative power fraction in `(0, 1)`. Default 0.95.
/// @param mr          Memory resource (nullptr → process default).
/// @return            Per-frame / per-column rolloff frequency.
Value spectralRolloffPoint(const Value &x, const Value &f,
                           double percentile = 0.95,
                           std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral decrease — slope-like measure relative to first bin.
///
/// @f$ d = \dfrac{1}{\sum_{k \ge 2} X_k}\,\sum_{k \ge 2} \dfrac{X_k - X_1}{k - 1} @f$
///
/// @param x   Time signal or spectrum.
/// @param f   Sample rate or frequency vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Per-frame / per-column decrease.
Value spectralDecrease(const Value &x, const Value &f,
                       std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral slope — least-squares slope of `(f, X)`.
///
/// @param x   Time signal or spectrum.
/// @param f   Sample rate or frequency vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Per-frame / per-column slope.
Value spectralSlope(const Value &x, const Value &f,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral flux — `p`-norm of frame-to-frame spectrum differences.
///
/// @param x   Time signal or spectrum.
/// @param f   Sample rate or frequency vector.
/// @param p   Norm order (default 2 — Euclidean).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `N - 1` flux values (one per frame transition).
Value spectralFlux(const Value &x, const Value &f, double p = 2.0,
                   std::pmr::memory_resource *mr = nullptr);

// ─────────────────────────────────────────────────────────────────────
// Shape statistics. Frequency moments (kurtosis / skewness) use the
// standard X-weighted central-moment definition.
// ─────────────────────────────────────────────────────────────────────

/// @brief Spectral crest — `max(X) / mean(X)` per frame.
///
/// @param x   Time signal or spectrum.
/// @param f   Sample rate or frequency vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Per-frame / per-column crest.
/// @see spectralFlatness
Value spectralCrest(const Value &x, const Value &f,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral entropy — Shannon entropy of normalised spectrum.
///
/// @param x   Time signal or spectrum.
/// @param f   Sample rate or frequency vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Per-frame / per-column normalised spectral entropy,
///            in `[0, 1]`.
Value spectralEntropy(const Value &x, const Value &f,
                      std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral flatness — `geomean(X) / mean(X)`.
///
/// `1` = white, `0` = tonal.
///
/// @param x   Time signal or spectrum.
/// @param f   Sample rate or frequency vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Per-frame / per-column flatness.
/// @see spectralCrest
Value spectralFlatness(const Value &x, const Value &f,
                       std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral kurtosis — X-weighted 4th central moment.
///
/// @param x   Time signal or spectrum.
/// @param f   Sample rate or frequency vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Per-frame / per-column kurtosis.
/// @see spectralSkewness, spectralSpread
Value spectralKurtosis(const Value &x, const Value &f,
                       std::pmr::memory_resource *mr = nullptr);

/// @brief Spectral skewness — X-weighted 3rd central moment.
///
/// @param x   Time signal or spectrum.
/// @param f   Sample rate or frequency vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Per-frame / per-column skewness.
/// @see spectralKurtosis, spectralSpread
Value spectralSkewness(const Value &x, const Value &f,
                       std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::audio
