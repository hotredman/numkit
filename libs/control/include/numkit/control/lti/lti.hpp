// libs/control/include/numkit/control/lti/lti.hpp
//
// LTI (Linear Time-Invariant) system constructors. Each returns a
// numkit struct value with a `kind` field tag ('tf', 'zpk', or 'ss')
// plus the canonical fields. Optional `Ts` field for discrete systems
// (>0); 0 means continuous-time.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// `tf(num, den [, Ts])` — transfer function in the variable s
/// (or z if Ts > 0). num/den are row coefficient vectors with the
/// leading coefficient first (MATLAB convention).
Value tf(std::pmr::memory_resource *mr,
         const Value &num, const Value &den, double Ts);

/// `zpk(z, p, k [, Ts])` — zero-pole-gain form.
Value zpk(std::pmr::memory_resource *mr,
          const Value &z, const Value &p, const Value &k, double Ts);

/// `ss(A, B, C, D [, Ts])` — state-space (continuous if Ts==0).
Value ss(std::pmr::memory_resource *mr,
         const Value &A, const Value &B,
         const Value &C, const Value &D, double Ts);

} // namespace numkit::control
