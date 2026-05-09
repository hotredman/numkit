// libs/comm/include/numkit/comm/modulation/generic_qam.hpp
//
// Generic constellation modulation/demodulation.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// `y = genqammod(x, constellation)` — modulate integer-coded
/// symbols `x ∈ [0, M-1]` by indexing into the user-supplied
/// `constellation` (length M, real or complex).
/// Output preserves input shape; type follows the constellation
/// (complex if it has any imaginary content). Bit-input mode
/// (`'InputType','bit'`) is deferred.
Value genqammod(std::pmr::memory_resource *mr, const Value &x,
                const Value &constellation);

/// `x = genqamdemod(y, constellation)` — return the integer index
/// in `[0, M-1]` of the constellation point closest (squared
/// Euclidean) to each entry of `y`. Output is real, same shape
/// as `y`.
Value genqamdemod(std::pmr::memory_resource *mr, const Value &y,
                  const Value &constellation);

} // namespace numkit::comm
