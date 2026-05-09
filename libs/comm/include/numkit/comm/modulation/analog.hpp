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

} // namespace numkit::comm
