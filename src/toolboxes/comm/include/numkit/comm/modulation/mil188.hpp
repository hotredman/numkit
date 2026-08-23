/// @file mil188.hpp
/// @ingroup group_comm
// toolboxes/comm/include/numkit/comm/modulation/mil188.hpp
//
// MIL-STD-188-110B/C QAM constellation modulation/demodulation.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::comm {

/// @brief MIL-STD-188-110 QAM modulator
/// (`y = mil188qammod(x, M)`).
///
/// Modulates integer indices using the MIL-STD-188-110 hard-coded
/// constellation table. Currently only `M = 16` is supported
/// (M = 32, 64, 256 deferred).
///
/// @param x   Integer symbol indices in `0..M-1`.
/// @param M   Modulation order (currently must be 16).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Complex baseband samples, same shape as `x`.
/// @throws Error  Unsupported `M`.
/// @see mil188qamdemod
Value mil188qammod(const Value &x, int M,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief MIL-STD-188-110 QAM demodulator
/// (`z = mil188qamdemod(y, M)`).
///
/// Inverts @ref mil188qammod via nearest-constellation-point search.
///
/// @param y   Received complex samples.
/// @param M   Modulation order (currently must be 16).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Integer symbol indices, same shape as `y`.
/// @throws Error  Unsupported `M`.
/// @see mil188qammod
Value mil188qamdemod(const Value &y, int M,
                     std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
