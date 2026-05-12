// libs/audio/include/numkit/audio/features/pitch_harmonics.hpp
//
// Audio Cycle E: pitch + harmonicRatio.
//
// MATLAB R2025b's `pitch` accepts 5 fundamental-frequency estimation
// methods. Each method has its own dedicated entry point in this
// header (pitch, pitchCEP, pitchPEF, pitchLHS, pitchSRH); `pitch` is
// the default (Method = 'NCF').

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::audio {

/// Fundamental frequency estimation — Normalised Cross-correlation Function (NCF).
///
/// Default MATLAB `pitch(x, fs)` method. Per-frame f0 estimation via
/// normalised autocorrelation, peak-picked within `[minF, maxF]`.
/// Defaults match MATLAB R2025b:
///   * `WindowLength`  = `round(0.052 · fs)`
///   * `OverlapLength` = `round(0.042 · fs)`
///   * `Range`         = `[50, 400] Hz`
///
/// @param x     Real 1-D audio signal.
/// @param fs    Sample rate in Hz.
/// @param minF  Minimum pitch search range in Hz. Default 50.
/// @param maxF  Maximum pitch search range in Hz. Default 400.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Column vector of per-frame f0 values in Hz.
///
/// @note MedianFilterLength / WindowLength / OverlapLength NV-pair args
///       are deferred. `pitchnn` (DNN-based) deferred (no DNN runtime).
///
/// @see pitchCEP, pitchPEF, pitchLHS, pitchSRH, harmonicRatio
Value pitch(const Value &                x,
            double                       fs,
            double                       minF = 50.0,
            double                       maxF = 400.0,
            std::pmr::memory_resource *  mr   = nullptr);

/// Pitch estimation via Cepstrum method (Noll 1967, MATLAB `Method='CEP'`).
/// @copydoc pitch
/// @see pitch
Value pitchCEP(const Value &                x,
               double                       fs,
               double                       minF = 50.0,
               double                       maxF = 400.0,
               std::pmr::memory_resource *  mr   = nullptr);

/// Pitch Estimation Filter method (Gonzalez & Brookes 2011, MATLAB `Method='PEF'`).
/// @copydoc pitch
Value pitchPEF(const Value &                x,
               double                       fs,
               double                       minF = 50.0,
               double                       maxF = 400.0,
               std::pmr::memory_resource *  mr   = nullptr);

/// Log Harmonic Sum (Subharmonic Summation, Hermes 1988, MATLAB `Method='LHS'`).
/// @copydoc pitch
Value pitchLHS(const Value &                x,
               double                       fs,
               double                       minF = 50.0,
               double                       maxF = 400.0,
               std::pmr::memory_resource *  mr   = nullptr);

/// Summation of Residual Harmonics (Drugman & Alwan 2011, MATLAB `Method='SRH'`).
///
/// LPC inverse filtering + harmonic summation on the residual spectrum.
/// @copydoc pitch
Value pitchSRH(const Value &                x,
               double                       fs,
               double                       minF = 50.0,
               double                       maxF = 400.0,
               std::pmr::memory_resource *  mr   = nullptr);

/// Harmonic ratio — strength of harmonic content per frame.
///
/// Computed as the maximum of the normalised autocorrelation function
/// over the search range, in `[0, 1]`. High values indicate periodic
/// (tonal) content; low values indicate noise.
///
/// Defaults match MATLAB R2025b:
///   * `WindowLength`  = `round(0.03 · fs)`
///   * `OverlapLength` = `round(0.02 · fs)`
///
/// @param x   Real 1-D audio signal.
/// @param fs  Sample rate in Hz.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of per-frame harmonic ratios in `[0, 1]`.
///
/// @see pitch
Value harmonicRatio(const Value &                x,
                    double                       fs,
                    std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::audio
