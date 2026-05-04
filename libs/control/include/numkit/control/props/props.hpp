// libs/control/include/numkit/control/props/props.hpp
//
// Predicates and analytic properties of an LTI struct value (tf,
// zpk, or ss as built by libs/control/lti). All operate on the
// `kind`-tagged struct produced by the cycle-31 constructors.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// `isct(sys)` — true when sys is a continuous-time model (Ts == 0).
Value isct(std::pmr::memory_resource *mr, const Value &sys);

/// `isdt(sys)` — true when sys is discrete-time (Ts > 0 or Ts == -1).
Value isdt(std::pmr::memory_resource *mr, const Value &sys);

/// `issiso(sys)` — single-input, single-output predicate.
Value issiso(std::pmr::memory_resource *mr, const Value &sys);

/// `isproper(sys)` — true when the transfer function is proper
/// (deg(num) ≤ deg(den)). For ss always true; for zpk numel(z) ≤ numel(p).
Value isproper(std::pmr::memory_resource *mr, const Value &sys);

/// `isstable(sys)` — continuous: all real(poles) < 0;
///                   discrete:   all |poles| < 1.
Value isstable(std::pmr::memory_resource *mr, const Value &sys);

/// `order(sys)` — system order (length of state vector for ss,
/// max(deg(num), deg(den)) for tf, max(numel(z), numel(p)) for zpk).
Value order(std::pmr::memory_resource *mr, const Value &sys);

/// `pole(sys)` — column vector of poles.
Value pole(std::pmr::memory_resource *mr, const Value &sys);

/// `zero(sys)` — column vector of (transmission) zeros.
Value zero(std::pmr::memory_resource *mr, const Value &sys);

/// `[wn, zeta, p] = damp(sys)` — natural frequency, damping ratio,
/// and pole list. Continuous-time only; discrete returns NaN for the
/// rotor pair (would need a Ts-dependent ln conversion).
void damp(std::pmr::memory_resource *mr, const Value &sys,
          Value *wn, Value *zeta, Value *p);

/// `[p, z] = pzmap(sys)` — pole-zero map (numeric form). Returns the
/// same arrays `pole(sys)` and `zero(sys)` produce, packaged together.
void pzmap(std::pmr::memory_resource *mr, const Value &sys,
           Value *pOut, Value *zOut);

/// `isstatic(sys)` — true when the system has no dynamics (order 0,
/// i.e. a pure gain).
Value isstatic(std::pmr::memory_resource *mr, const Value &sys);

/// `z = tzero(sys)` — transmission zeros. For SISO inputs this is
/// equivalent to `zero(sys)`. Throws on MIMO (would need a
/// generalized eigenproblem we don't yet expose).
Value tzero(std::pmr::memory_resource *mr, const Value &sys);

} // namespace numkit::control
