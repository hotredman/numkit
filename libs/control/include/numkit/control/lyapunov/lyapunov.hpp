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
Value lyap(const Value &A, const Value &Q, std::pmr::memory_resource *mr = nullptr);

/// Discrete Lyapunov solver.
Value dlyap(const Value &A, const Value &Q, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
