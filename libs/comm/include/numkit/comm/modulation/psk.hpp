// libs/comm/include/numkit/comm/modulation/psk.hpp
//
// PSK / DPSK modulator and demodulator. Function-form (no
// `comm.PSKModulator` System Object).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// pskmod(x, M[, ini_phase, symbol_order]) — M-ary PSK modulator.
/// `x`: integer symbols in 0..M-1 (or bit groups, see decode flag).
/// `ini_phase` rad shift applied to all symbols (default 0).
/// `symbol_order` ∈ {"bin", "gray"}; default "gray" matches MATLAB.
/// Returns complex array same shape as x.
Value pskmod(std::pmr::memory_resource *mr, const Value &x, int M,
             double ini_phase, const std::string &symbol_order);

/// pskdemod(y, M[, ini_phase, symbol_order]) — nearest-phase decision.
Value pskdemod(std::pmr::memory_resource *mr, const Value &y, int M,
               double ini_phase, const std::string &symbol_order);

/// dpskmod(x, M[, phase_rot, symbol_order]) — differential PSK.
/// First symbol uses an initial reference phase (default phase_rot);
/// each subsequent input adds a phase increment determined by the symbol.
Value dpskmod(std::pmr::memory_resource *mr, const Value &x, int M,
              double phase_rot, const std::string &symbol_order);

/// dpskdemod(y, M[, phase_rot, symbol_order]) — phase-difference decoder.
Value dpskdemod(std::pmr::memory_resource *mr, const Value &y, int M,
                double phase_rot, const std::string &symbol_order);

} // namespace numkit::comm
