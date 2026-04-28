// libs/builtin/include/numkit/m/builtin/MStdPoly.hpp
//
// Polynomial-domain builtins. roots now; polyder / polyint / polyval
// later in the round.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

/// roots(p) — finds the roots of the polynomial whose coefficients are
/// in p (MATLAB convention: p(1) is the leading coefficient, p(end) is
/// the constant term). Returns a column vector of (possibly complex)
/// roots. Uses the shared Durand-Kerner solver.
///
/// Behaviour:
///   * Empty / scalar input → 0×1 column.
///   * Real polynomial → output column is COMPLEX if any root has a
///     non-trivial imaginary part; otherwise DOUBLE.
///   * Trailing zeros in p → corresponding number of roots at 0.
MValue roots(Allocator &alloc, const MValue &p);

} // namespace numkit::m::builtin
