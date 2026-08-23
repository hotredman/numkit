/// @file generic_qam.hpp
/// @ingroup group_comm
// toolboxes/comm/include/numkit/comm/modulation/generic_qam.hpp
//
// Generic constellation modulation/demodulation.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::comm {

/// @brief Generic constellation modulator
/// (`y = genqammod(x, constellation)`).
///
/// Maps integer-coded symbols `x ∈ [0, M-1]` by indexing into the
/// user-supplied `constellation` (length M, real or complex).
/// Output preserves the input shape; type follows the constellation
/// (complex if it has any imaginary content). Bit-input mode
/// (`'InputType', 'bit'`) is deferred.
///
/// @param x              Integer symbol stream.
/// @param constellation  Length-M constellation table.
/// @param mr             Memory resource (nullptr → process default).
/// @return               Modulated samples, same shape as `x`.
/// @see genqamdemod
Value genqammod(const Value &x, const Value &constellation,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Generic constellation demodulator
/// (`x = genqamdemod(y, constellation)`).
///
/// Returns the integer index in `[0, M-1]` of the constellation
/// point closest (squared Euclidean) to each entry of `y`.
///
/// @param y              Received samples.
/// @param constellation  Length-M constellation table.
/// @param mr             Memory resource (nullptr → process default).
/// @return               Integer indices, real, same shape as `y`.
/// @see genqammod
Value genqamdemod(const Value &y, const Value &constellation,
                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
