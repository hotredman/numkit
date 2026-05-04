// libs/comm/include/numkit/comm/modulation/qam.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::comm {

/// pammod(x, M[, ini_phase, symbol_order]) — M-ary PAM. Maps symbol
/// s ∈ 0..M-1 to a real-valued amplitude in {-(M-1), ..., -1, 1, ..., (M-1)}.
/// MATLAB default: gray ordering, no power normalisation.
Value pammod(std::pmr::memory_resource *mr, const Value &x, int M,
             double ini_phase, const std::string &symbol_order);

/// pamdemod(y, M[, ini_phase, symbol_order]) — nearest-amplitude decision.
Value pamdemod(std::pmr::memory_resource *mr, const Value &y, int M,
               double ini_phase, const std::string &symbol_order);

/// qammod(x, M[, symbol_order, 'UnitAveragePower', flag]) — rectangular
/// M-ary QAM. Requires M = K² (perfect square); for non-square M we fall
/// back to a rectangular grid floor(√M) × ceil(M/floor(√M)).
Value qammod(std::pmr::memory_resource *mr, const Value &x, int M,
             const std::string &symbol_order, bool unit_power);

/// qamdemod(y, M[, symbol_order, 'UnitAveragePower', flag]) — nearest
/// constellation point.
Value qamdemod(std::pmr::memory_resource *mr, const Value &y, int M,
               const std::string &symbol_order, bool unit_power);

/// modnorm(ref, type, target) — scaling factor so that ref·factor has
/// the requested 'avpow' or 'peakpow'.
Value modnorm(std::pmr::memory_resource *mr, const Value &ref,
              const std::string &type, double target);

} // namespace numkit::comm
