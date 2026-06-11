// toolboxes/audio/include/numkit/audio/features/pitch_harmonics.hpp
//
// Pitch + harmonicRatio.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::audio {

/// @file
/// @brief Fundamental-frequency estimation and harmonic-content metrics.
///
/// Five fundamental-frequency estimators, one dedicated entry point per
/// algorithm — `pitch` (normalised cross-correlation, the default),
/// `pitchCEP`, `pitchPEF`, `pitchLHS`, `pitchSRH`.
///
/// **Shared semantics** across the five estimators:
/// - `x` is a real 1-D audio signal, `fs` the sample rate in Hz.
/// - `minF` / `maxF` bound the pitch search range (defaults 50 / 400 Hz).
/// - Output is a column vector of per-frame `f0` values in Hz.
/// - `MedianFilterLength` / `WindowLength` / `OverlapLength` NV-pair
///   args are deferred. `pitchnn` (DNN-based) deferred.

/// @brief Pitch via the Normalised Cross-correlation Function
/// (the default estimator).
///
/// Per-frame f0 estimation via normalised autocorrelation, peak-picked
/// within `[minF, maxF]`. Window defaults:
/// - `WindowLength`  = `round(0.052 · fs)`
/// - `OverlapLength` = `round(0.042 · fs)`
/// - `Range`         = `[50, 400] Hz`
///
/// @param x     Real 1-D audio signal.
/// @param fs    Sample rate in Hz.
/// @param minF  Minimum pitch search bound in Hz (default 50).
/// @param maxF  Maximum pitch search bound in Hz (default 400).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Column vector of per-frame `f0` in Hz.
/// @see pitchCEP, pitchPEF, pitchLHS, pitchSRH, harmonicRatio
Value pitch(const Value &x, double fs,
            double minF = 50.0, double maxF = 400.0,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Pitch via the cepstrum method (Noll 1967).
///
/// Same arg semantics as @ref pitch.
///
/// @param x     See @ref pitch.
/// @param fs    See @ref pitch.
/// @param minF  See @ref pitch.
/// @param maxF  See @ref pitch.
/// @param mr    Memory resource (nullptr → process default).
/// @return                Column vector of per-frame `f0` in Hz.
/// @see pitch
Value pitchCEP(const Value &x, double fs,
               double minF = 50.0, double maxF = 400.0,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Pitch Estimation Filter
/// (Gonzalez & Brookes 2011).
///
/// Same arg semantics as @ref pitch.
///
/// @param x     See @ref pitch.
/// @param fs    See @ref pitch.
/// @param minF  See @ref pitch.
/// @param maxF  See @ref pitch.
/// @param mr    Memory resource (nullptr → process default).
/// @return                Column vector of per-frame `f0` in Hz.
/// @see pitch
Value pitchPEF(const Value &x, double fs,
               double minF = 50.0, double maxF = 400.0,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Log Harmonic Sum / Subharmonic Summation
/// (Hermes 1988).
///
/// Same arg semantics as @ref pitch.
///
/// @param x     See @ref pitch.
/// @param fs    See @ref pitch.
/// @param minF  See @ref pitch.
/// @param maxF  See @ref pitch.
/// @param mr    Memory resource (nullptr → process default).
/// @return                Column vector of per-frame `f0` in Hz.
/// @see pitch
Value pitchLHS(const Value &x, double fs,
               double minF = 50.0, double maxF = 400.0,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Summation of Residual Harmonics
/// (Drugman & Alwan 2011).
///
/// LPC inverse filtering + harmonic summation on the residual spectrum.
/// Same arg semantics as @ref pitch.
///
/// @param x     See @ref pitch.
/// @param fs    See @ref pitch.
/// @param minF  See @ref pitch.
/// @param maxF  See @ref pitch.
/// @param mr    Memory resource (nullptr → process default).
/// @return                Column vector of per-frame `f0` in Hz.
/// @see pitch
Value pitchSRH(const Value &x, double fs,
               double minF = 50.0, double maxF = 400.0,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Harmonic ratio — strength of harmonic content per frame.
///
/// Computed as the maximum of the normalised autocorrelation function
/// over the search range, clipped to `[0, 1]`. High values indicate
/// periodic (tonal) content; low values indicate noise. Window
/// defaults: `WindowLength = round(0.03 · fs)`,
/// `OverlapLength = round(0.02 · fs)`.
///
/// @param x   Real 1-D audio signal.
/// @param fs  Sample rate in Hz.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of per-frame harmonic ratios in `[0, 1]`.
/// @see pitch
Value harmonicRatio(const Value &x, double fs,
                    std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::audio
