// libs/comm/include/numkit/comm/modulation/apsk.hpp
//
// Amplitude-Phase Shift Keying multi-ring constellation.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// `y = apskmod(x, M, radii [, phaseoffset [, SymbolMapping]])`
/// — modulate integer indices `x` by indexing into a multi-ring
/// PSK constellation. Default phaseoffset = pi./M (per-ring).
/// Default SymbolMapping = identity 0..sum(M)-1; pass an explicit
/// permutation to reorder. Gray default deferred (MATLAB's per-ring
/// Gray for arbitrary M needs more probing).
Value apskmod(std::pmr::memory_resource *mr, const Value &x,
              const Value &M, const Value &radii,
              const Value *phaseoffset, const Value *mapping);

/// `z = apskdemod(y, M, radii [, phaseoffset [, SymbolMapping]])`
/// — invert apskmod via nearest-constellation-point search.
Value apskdemod(std::pmr::memory_resource *mr, const Value &y,
                const Value &M, const Value &radii,
                const Value *phaseoffset, const Value *mapping);

} // namespace numkit::comm
