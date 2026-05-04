// libs/control/include/numkit/control/state/state.hpp
//
// State-space structural primitives: controllability and
// observability matrices, plus rank-based "is" predicates.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// `Co = ctrb(A, B)` or `ctrb(sys)` — controllability matrix
///   C_o = [B, A·B, A²·B, …, A^(n−1)·B]   shape n × (n·m).
Value ctrb_AB(std::pmr::memory_resource *mr,
              const Value &A, const Value &B);
Value ctrb_sys(std::pmr::memory_resource *mr, const Value &sys);

/// `O = obsv(A, C)` or `obsv(sys)` — observability matrix
///   O_b = [C; C·A; C·A²; …; C·A^(n−1)]   shape (n·p) × n.
Value obsv_AC(std::pmr::memory_resource *mr,
              const Value &A, const Value &C);
Value obsv_sys(std::pmr::memory_resource *mr, const Value &sys);

} // namespace numkit::control
