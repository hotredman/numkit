// toolboxes/comm/include/numkit/comm/modulation/analog.hpp
//
// Analog modulators (PM / AM / FM / SSB / MSK).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::comm {

/// @brief Phase modulator
/// (`y = pmmod(x, fc, fs, phasedev, ini_phase)`).
///
/// `y = cos(2π·fc·t + phasedev·x + ini_phase)` evaluated on
/// `t = (0, 1/fs, 2/fs, …)`. Output preserves the input shape;
/// row-vector input round-trips as a row vector.
///
/// @param x          Real input message.
/// @param fc         Carrier frequency in Hz.
/// @param fs         Sample rate in Hz.
/// @param phasedev   Phase-deviation factor (rad per unit of `x`).
/// @param ini_phase  Initial carrier phase in radians.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Modulated signal, same shape as `x`.
/// @see ammod, fmmod
Value pmmod(const Value &x, double fc, double fs, double phasedev,
            double ini_phase,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Amplitude modulator
/// (`y = ammod(x, fc, fs, ini_phase, carr_amp)`).
///
/// `y = (x + carr_amp) · cos(2π·fc·t + ini_phase)`. `carr_amp == 0`
/// gives DSB-SC (suppressed carrier); `carr_amp != 0` gives DSB-TC
/// (transmitted carrier).
///
/// @param x          Real input message.
/// @param fc         Carrier frequency in Hz.
/// @param fs         Sample rate in Hz.
/// @param ini_phase  Initial carrier phase in radians.
/// @param carr_amp   Carrier amplitude (0 → DSB-SC).
/// @param mr         Memory resource (nullptr → process default).
/// @return           Modulated signal, same shape as `x`.
/// @see pmmod, fmmod, ssbmod
Value ammod(const Value &x, double fc, double fs, double ini_phase,
            double carr_amp,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Frequency modulator
/// (`y = fmmod(x, fc, fs, freqdev, ini_phase)`).
///
/// `int_x = cumsum(x)/fs`; `y = cos(2π·fc·t + 2π·freqdev·int_x +
/// ini_phase)`.
///
/// @param x          Real input message.
/// @param fc         Carrier frequency in Hz.
/// @param fs         Sample rate in Hz.
/// @param freqdev    Frequency-deviation factor (Hz per unit of `x`).
/// @param ini_phase  Initial carrier phase in radians.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Modulated signal, same shape as `x`.
/// @see ammod, pmmod, ssbmod
Value fmmod(const Value &x, double fc, double fs, double freqdev,
            double ini_phase,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Single-sideband modulator
/// (`y = ssbmod(x, fc, fs, ini_phase, upper)`).
///
/// `y = x·cos(2π·fc·t + ini_phase) ± imag(hilbert(x))·
/// sin(2π·fc·t + ini_phase)` with `+` for lower sideband (default,
/// `upper = false`) and `−` for upper.
///
/// @param x          Real input message.
/// @param fc         Carrier frequency in Hz.
/// @param fs         Sample rate in Hz.
/// @param ini_phase  Initial carrier phase in radians.
/// @param upper      `true` → upper sideband, `false` → lower.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Modulated signal, same shape as `x`.
/// @see ammod
Value ssbmod(const Value &x, double fc, double fs, double ini_phase,
             bool upper,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Minimum-shift-keying modulator (differential variant)
/// (`y = mskmod(x, nSamp, ini_phase)`).
///
/// Output length is `numel(x) · nSamp`. Linear-phase ramp produces
/// the continuous-phase MSK constellation on the unit circle.
/// Non-differential variant deferred.
///
/// @param x          Symbol stream.
/// @param nSamp      Samples per symbol.
/// @param ini_phase  Initial carrier phase in radians.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Complex baseband waveform of length
///                   `numel(x) · nSamp`.
Value mskmod(const Value &x, int nSamp, double ini_phase,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
