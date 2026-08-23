/// @file analog.hpp
/// @ingroup group_comm
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

/// @brief Phase demodulator — inverse of @ref pmmod
/// (`x = pmdemod(y, fc, fs, phasedev, ini_phase)`).
///
/// `x = angle(hilbert(y)·exp(−j·(2π·fc·t + ini_phase))) / phasedev` — the
/// instantaneous phase of the analytic signal, down-converted by the
/// carrier, scaled by the phase-deviation constant.
///
/// @param y          Modulated signal.
/// @param fc         Carrier frequency in Hz.
/// @param fs         Sample rate in Hz.
/// @param phasedev   Phase-deviation constant (radians per unit message).
/// @param ini_phase  Initial carrier phase in radians.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Recovered message, same shape as `y`.
/// @see pmmod, fmdemod
Value pmdemod(const Value &y, double fc, double fs, double phasedev,
              double ini_phase, std::pmr::memory_resource *mr = nullptr);

/// @brief Frequency demodulator — inverse of @ref fmmod
/// (`x = fmdemod(y, fc, fs, freqdev, ini_phase)`).
///
/// Differentiates the unwrapped instantaneous phase of the down-converted
/// analytic signal: `x = [0, diff(unwrap(φ))]·fs / (2π·freqdev)` where
/// `φ = angle(hilbert(y)·exp(−j·(2π·fc·t + ini_phase)))`.
///
/// @param y          Modulated signal.
/// @param fc         Carrier frequency in Hz.
/// @param fs         Sample rate in Hz.
/// @param freqdev    Frequency-deviation factor (Hz per unit message).
/// @param ini_phase  Initial carrier phase in radians.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Recovered message, same shape as `y`.
/// @see fmmod, pmdemod
Value fmdemod(const Value &y, double fc, double fs, double freqdev,
              double ini_phase, std::pmr::memory_resource *mr = nullptr);

/// @brief Amplitude demodulator — inverse of @ref ammod
/// (`x = amdemod(y, fc, fs, ini_phase, carramp)`).
///
/// Coherent (synchronous) detection: multiply by the carrier and zero-phase
/// low-pass — `x = 2·filtfilt(butter(5, fc·2/fs), y·cos(2π·fc·t +
/// ini_phase)) − carramp`. (`carramp` is the transmitted-carrier amplitude;
/// the default 0 is double-sideband suppressed-carrier.)
///
/// @param y          Modulated signal.
/// @param fc         Carrier frequency in Hz.
/// @param fs         Sample rate in Hz.
/// @param ini_phase  Initial carrier phase in radians.
/// @param carr_amp   Transmitted-carrier amplitude (subtracted after detect).
/// @param mr         Memory resource (nullptr → process default).
/// @return           Recovered message, same shape as `y`.
/// @see ammod, ssbdemod
Value amdemod(const Value &y, double fc, double fs, double ini_phase,
              double carr_amp, std::pmr::memory_resource *mr = nullptr);

/// @brief Single-sideband demodulator — inverse of @ref ssbmod
/// (`x = ssbdemod(y, fc, fs, ini_phase)`).
///
/// Coherent detection: `x = 2·filtfilt(butter(5, fc·2/fs), y·cos(2π·fc·t +
/// ini_phase))`. Recovers the message regardless of which sideband was
/// transmitted.
///
/// @param y          Modulated signal.
/// @param fc         Carrier frequency in Hz.
/// @param fs         Sample rate in Hz.
/// @param ini_phase  Initial carrier phase in radians.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Recovered message, same shape as `y`.
/// @see ssbmod, amdemod
Value ssbdemod(const Value &y, double fc, double fs, double ini_phase,
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

/// @brief Minimum-shift-keying demodulator (differential variant)
/// (`z = mskdemod(y, nSamp [, ini_phase])`).
///
/// Coherent inverse of @ref mskmod. Each bit is the sign of the symbol's
/// accumulated phase increment: `bit_k = (Σ_{within symbol k}
/// angle(y[n]·conj(y[n-1])) > 0)`. Because the decision uses phase
/// *increments*, it is robust to a constant phase rotation and to noise,
/// and `ini_phase` does not affect the bits (it only sets the returned
/// final-phase state). Output has `numel(y) / nSamp` bits per channel, in
/// the input's row/column orientation. Non-differential variant deferred
/// (matching @ref mskmod).
///
/// @param y          Complex baseband MSK waveform (length a multiple of nSamp).
/// @param nSamp      Samples per symbol.
/// @param ini_phase  Initial carrier phase (radians); only feeds @p phase_out.
/// @param phase_out  If non-null, receives the final phase state in [0, 2π).
/// @param mr         Memory resource (nullptr → process default).
/// @return           Demodulated bit stream ({0,1}).
Value mskdemod(const Value &y, int nSamp, double ini_phase,
               double *phase_out = nullptr,
               std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
