// libs/signal/include/numkit/signal/filter_analysis/predicates.hpp
//
// Boolean classifiers for digital filters: isfir / isallpass / isstable /
// islinphase / isminphase / ismaxphase. All take (b) or (b, a) and
// return a logical scalar. Tolerances follow MATLAB's defaults.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// isfir(b) / isfir(b, a) — true when the filter is FIR (a is scalar 1
/// or a vector with only a(0) ~= 0). Tolerance is 1e-12.
bool isfir(const Value &b);
bool isfir(const Value &b, const Value &a);

/// isstable(b, a) — true when every pole of A(z) is strictly inside
/// the unit circle (radius < 1 - tol).
bool isstable(std::pmr::memory_resource *mr, const Value &b, const Value &a);

/// isminphase(b, a) — true when every zero of B(z) is inside the unit
/// circle AND the filter is stable.
bool isminphase(std::pmr::memory_resource *mr, const Value &b, const Value &a);

/// ismaxphase(b, a) — true when every zero of B(z) is OUTSIDE the unit
/// circle (radius > 1 + tol).
bool ismaxphase(std::pmr::memory_resource *mr, const Value &b, const Value &a);

/// islinphase(b, a) — true when the filter has linear phase. For FIR
/// this requires b symmetric (b == flip(b)) or antisymmetric. IIR with
/// non-trivial denominator returns false (linear phase is impossible).
bool islinphase(const Value &b, const Value &a);

/// isallpass(b, a) — true when |H(e^{jw})| is constant. Equivalent
/// algebraic test: a == flip(b) (within tol).
bool isallpass(const Value &b, const Value &a);

} // namespace numkit::signal
