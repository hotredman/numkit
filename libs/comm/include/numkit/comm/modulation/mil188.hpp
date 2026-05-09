// libs/comm/include/numkit/comm/modulation/mil188.hpp
//
// MIL-STD-188-110B/C QAM constellation modulation/demodulation.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// `y = mil188qammod(x, M)` — modulate integer indices using the
/// MIL-STD-188-110 hard-coded constellation table for the given M.
/// Currently only M = 16 is supported (M = 32, 64, 256 deferred).
Value mil188qammod(std::pmr::memory_resource *mr, const Value &x, int M);

/// `z = mil188qamdemod(y, M)` — invert mil188qammod via nearest-
/// constellation-point search.
Value mil188qamdemod(std::pmr::memory_resource *mr, const Value &y, int M);

} // namespace numkit::comm
