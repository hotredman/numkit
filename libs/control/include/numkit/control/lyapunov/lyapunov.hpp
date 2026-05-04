// libs/control/include/numkit/control/lyapunov/lyapunov.hpp
//
// Continuous and discrete Lyapunov equations.
//
//   lyap (A, Q): A·X + X·Aᵀ + Q = 0
//   dlyap(A, Q): A·X·Aᵀ − X + Q = 0
//
// We solve the n²×n² vectorised system directly — fine for the
// state dims that show up in textbook control problems (n ≤ 32).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// Continuous Lyapunov solver.
Value lyap(std::pmr::memory_resource *mr, const Value &A, const Value &Q);

/// Discrete Lyapunov solver.
Value dlyap(std::pmr::memory_resource *mr, const Value &A, const Value &Q);

} // namespace numkit::control
