// libs/control/include/numkit/control/state/place.hpp
//
// Pole-placement state-feedback design via Ackermann's formula.
// SISO only — multi-input MATLAB `place` uses Kautsky–Nichols
// eigenvector assignment, which we don't support here.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// `K = acker(A, B, p)` — Ackermann's formula for SISO pole placement.
/// `p` is an n-vector of desired closed-loop pole locations.
/// Returns a 1×n row K such that  eig(A − B·K) = p.
Value acker(std::pmr::memory_resource *mr,
            const Value &A, const Value &B, const Value &p);

/// `K = place(A, B, p)` — alias for `acker` in our SISO build.
/// MATLAB's robust multi-input variant is intentionally NOT
/// re-implemented here.
Value place(std::pmr::memory_resource *mr,
            const Value &A, const Value &B, const Value &p);

} // namespace numkit::control
