// toolboxes/signal/src/transforms/extras_detail.hpp
//
// Private (src-only) helper shared between the engine-free compute in
// extras.cpp (bitrevorder) and its CallContext register half in extras_reg.cpp
// (bitrevorder_reg, which also emits the 1-based permutation index as a 2nd
// output). NOT part of the public signal API.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <cstddef>

namespace numkit::signal {

// Reverse the low `bits` bits of `v` (the bit-reversal permutation index).
std::size_t bitReverse(std::size_t v, std::size_t bits);

} // namespace numkit::signal
