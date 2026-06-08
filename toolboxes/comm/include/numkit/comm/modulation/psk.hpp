// toolboxes/comm/include/numkit/comm/modulation/psk.hpp
//
// PSK / DPSK modulator and demodulator. Function-form (no
// `comm.PSKModulator` System Object).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit::comm {

/// @brief M-ary PSK modulator
/// (`y = pskmod(x, M, ini_phase, symbol_order)`).
///
/// @param x             Integer symbols in `0..M-1`.
/// @param M             Modulation order.
/// @param ini_phase     Phase shift in radians applied to all symbols
///                      (default 0).
/// @param symbol_order  `"gray"` (default) or
///                      `"bin"`.
/// @param mr            Memory resource (nullptr → process default).
/// @return              Complex baseband samples, same shape as `x`.
/// @see pskdemod, dpskmod, qammod
Value pskmod(const Value &x, int M, double ini_phase,
             const std::string &symbol_order,
             std::pmr::memory_resource *mr = nullptr);

/// @brief M-ary PSK demodulator — nearest-phase decision
/// (`x = pskdemod(y, M, ini_phase, symbol_order)`).
///
/// @param y             Received complex samples.
/// @param M             Modulation order.
/// @param ini_phase     Phase reference used at modulation time.
/// @param symbol_order  `"gray"` (default) or `"bin"`.
/// @param mr            Memory resource (nullptr → process default).
/// @return              Integer symbol indices, same shape as `y`.
/// @see pskmod
Value pskdemod(const Value &y, int M, double ini_phase,
               const std::string &symbol_order,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Differential PSK modulator
/// (`y = dpskmod(x, M, phase_rot, symbol_order)`).
///
/// First symbol uses an initial reference phase (default
/// `phase_rot`); each subsequent input adds a phase increment
/// determined by the symbol.
///
/// @param x             Integer symbols in `0..M-1`.
/// @param M             Modulation order.
/// @param phase_rot     Per-step phase rotation in radians.
/// @param symbol_order  `"gray"` (default) or `"bin"`.
/// @param mr            Memory resource (nullptr → process default).
/// @return              Complex baseband samples, same shape as `x`.
/// @see dpskdemod, pskmod
Value dpskmod(const Value &x, int M, double phase_rot,
              const std::string &symbol_order,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Differential PSK demodulator — phase-difference decoder
/// (`x = dpskdemod(y, M, phase_rot, symbol_order)`).
///
/// @param y             Received complex samples.
/// @param M             Modulation order.
/// @param phase_rot     Same per-step rotation used at modulation.
/// @param symbol_order  `"gray"` (default) or `"bin"`.
/// @param mr            Memory resource (nullptr → process default).
/// @return              Integer symbol indices, same shape as `y`.
/// @see dpskmod
Value dpskdemod(const Value &y, int M, double phase_rot,
                const std::string &symbol_order,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
