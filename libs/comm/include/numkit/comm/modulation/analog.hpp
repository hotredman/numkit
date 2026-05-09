// libs/comm/include/numkit/comm/modulation/analog.hpp
//
// Analog modulators (pmmod for now; ammod/fmmod/ssbmod planned).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// `y = pmmod(x, Fc, Fs, phasedev [, ini_phase])` — phase modulator.
///   y = cos(2π·Fc·t + phasedev·x + ini_phase)
/// where t = (0, 1/Fs, 2/Fs, ...). Output preserves input shape;
/// row-vector input round-trips as a row vector. Bit-equal with
/// MATLAB R2025b.
Value pmmod(std::pmr::memory_resource *mr, const Value &x,
            double fc, double fs, double phasedev, double ini_phase);

/// `y = ammod(x, Fc, Fs [, ini_phase [, carr_amp]])` — amplitude
/// modulator.
///   y = (x + carr_amp) · cos(2π·Fc·t + ini_phase)
/// carr_amp == 0 → DSB-SC (suppressed carrier);
/// carr_amp != 0 → DSB-TC (transmitted carrier).
/// Row-vector input round-trips as a row vector. Bit-equal with
/// MATLAB R2025b.
Value ammod(std::pmr::memory_resource *mr, const Value &x,
            double fc, double fs, double ini_phase, double carr_amp);

/// `y = fmmod(x, Fc, Fs, freqdev [, ini_phase])` — frequency
/// modulator.
///   int_x = cumsum(x) / Fs
///   y     = cos(2π·Fc·t + 2π·freqdev·int_x + ini_phase)
/// Row-vector input round-trips as a row vector. Bit-equal with
/// MATLAB R2025b.
Value fmmod(std::pmr::memory_resource *mr, const Value &x,
            double fc, double fs, double freqdev, double ini_phase);

/// `y = ssbmod(x, Fc, Fs [, ini_phase [, 'upper']])` — single-
/// sideband modulator.
///   y = x·cos(2π·Fc·t + ini_phase)
///       + sign·imag(hilbert(x))·sin(2π·Fc·t + ini_phase)
/// where sign = +1 for the default lower sideband, −1 for upper.
/// Row-vector input round-trips as a row vector. Bit-equal with
/// MATLAB R2025b.
Value ssbmod(std::pmr::memory_resource *mr, const Value &x,
             double fc, double fs, double ini_phase, bool upper);

/// `y = mskmod(x, nSamp [, ini_phase])` — minimum-shift keying
/// modulator (differential variant only). Output length is
/// `numel(x) * nSamp`. Linear-phase ramp produces the
/// continuous-phase MSK constellation on the unit circle.
/// Non-differential variant deferred.
Value mskmod(std::pmr::memory_resource *mr, const Value &x,
             int nSamp, double ini_phase);

} // namespace numkit::comm
