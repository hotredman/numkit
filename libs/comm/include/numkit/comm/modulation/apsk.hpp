// libs/comm/include/numkit/comm/modulation/apsk.hpp
//
// Amplitude-Phase Shift Keying multi-ring constellation.

#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit::comm {

/// `y = apskmod(x, M, radii [, phaseoffset [, SymbolMapping]])`
/// — modulate integer indices `x` by indexing into a multi-ring
/// PSK constellation. `M` and `radii` describe the per-ring constellation
/// (must have matching length). Default phaseoffset = pi./M (per-ring).
/// Default SymbolMapping = identity 0..sum(M)-1; pass an explicit
/// permutation to reorder. Gray default deferred (MATLAB's per-ring
/// Gray for arbitrary M needs more probing).
Value apskmod(const Value &x, Span<const size_t> M, Span<const double> radii,
              const Value &phaseoffset = Value::Empty,
              const Value &mapping = Value::Empty,
              std::pmr::memory_resource *mr = nullptr);

/// `z = apskdemod(y, M, radii [, phaseoffset [, SymbolMapping]])`
/// — invert apskmod via nearest-constellation-point search.
Value apskdemod(const Value &y, Span<const size_t> M, Span<const double> radii,
                const Value &phaseoffset = Value::Empty,
                const Value &mapping = Value::Empty,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
