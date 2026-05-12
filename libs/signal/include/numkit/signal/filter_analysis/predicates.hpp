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
/// @copydoc isfir(const Value &)
bool isfir(const Value &b, const Value &a);

/// isstable(b, a) — true when every pole of A(z) is strictly inside
/// the unit circle (radius < 1 - tol).
bool isstable(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// isminphase(b, a) — true when every zero of B(z) is inside the unit
/// circle AND the filter is stable.
bool isminphase(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// ismaxphase(b, a) — true when every zero of B(z) is OUTSIDE the unit
/// circle (radius > 1 + tol).
bool ismaxphase(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// islinphase(b, a) — true when the filter has linear phase. For FIR
/// this requires b symmetric (b == flip(b)) or antisymmetric. IIR with
/// non-trivial denominator returns false (linear phase is impossible).
bool islinphase(const Value &b, const Value &a);

/// isallpass(b, a) — true when |H(e^{jw})| is constant. Equivalent
/// algebraic test: a == flip(b) (within tol).
bool isallpass(const Value &b, const Value &a);

/// filtord(b) / filtord(b, a) — filter order. Returns:
///   FIR (a omitted or [1]): length(b_trimmed) - 1
///   IIR:                    max(length(b_trimmed), length(a_trimmed)) - 1
/// Trailing zeros are trimmed before counting. Returns int (as double).
int filtord(const Value &b);
/// @copydoc filtord(const Value &)
int filtord(const Value &b, const Value &a);

/// firtype(b) — FIR filter type per MATLAB convention:
///   Type 1: even order (odd length), b symmetric
///   Type 2: odd  order (even length), b symmetric
///   Type 3: even order (odd length), b antisymmetric
///   Type 4: odd  order (even length), b antisymmetric
/// Throws if b is neither symmetric nor antisymmetric within tol.
int firtype(const Value &b);

/// filternorm(b, a [, pnorm]) — filter L_p norm.
///   pnorm = 2   (default): sqrt((1/π) ∫_0^π |H(e^{jw})|² dw)
///   pnorm = inf:           max_w |H(e^{jw})|
/// Both via freqz on a default 8192-point grid.
double filternorm(const Value &b, const Value &a, double pnorm = 2.0, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
