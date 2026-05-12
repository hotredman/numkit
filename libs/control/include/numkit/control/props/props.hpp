// libs/control/include/numkit/control/props/props.hpp
//
// Predicates and analytic properties of an LTI struct value (tf,
// zpk, or ss as built by libs/control/lti). All operate on the
// `kind`-tagged struct produced by the cycle-31 constructors.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <utility>

namespace numkit::control {

/// `isct(sys)` — true when sys is a continuous-time model (Ts == 0).
Value isct(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `isdt(sys)` — true when sys is discrete-time (Ts > 0 or Ts == -1).
Value isdt(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `issiso(sys)` — single-input, single-output predicate.
Value issiso(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `isproper(sys)` — true when the transfer function is proper
/// (deg(num) ≤ deg(den)). For ss always true; for zpk numel(z) ≤ numel(p).
Value isproper(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `isstable(sys)` — continuous: all real(poles) < 0;
///                   discrete:   all |poles| < 1.
Value isstable(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `order(sys)` — system order (length of state vector for ss,
/// max(deg(num), deg(den)) for tf, max(numel(z), numel(p)) for zpk).
Value order(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `pole(sys)` — column vector of poles.
Value pole(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `zero(sys)` — column vector of (transmission) zeros.
Value zero(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Result of `damp(sys)` — `[wn, zeta, p] = damp(sys)`.
struct DampResult {
    Value wn;     ///< natural frequencies (column vector)
    Value zeta;   ///< damping ratios     (column vector)
    Value p;      ///< pole list           (column vector)
};

/// `[wn, zeta, p] = damp(sys)` — natural frequency, damping ratio,
/// and pole list. Continuous-time only; discrete returns NaN for the
/// rotor pair (would need a Ts-dependent ln conversion).
DampResult damp(const Value &sys,
                std::pmr::memory_resource *mr = nullptr);

/// `[p, z] = pzmap(sys)` — pole-zero map (numeric form). Returns the
/// same arrays `pole(sys)` and `zero(sys)` produce, packaged together.
std::pair<Value, Value>
pzmap(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `isstatic(sys)` — true when the system has no dynamics (order 0,
/// i.e. a pure gain).
Value isstatic(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `z = tzero(sys)` — transmission zeros. For SISO inputs this is
/// equivalent to `zero(sys)`. Throws on MIMO (would need a
/// generalized eigenproblem we don't yet expose).
Value tzero(const Value &sys, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
