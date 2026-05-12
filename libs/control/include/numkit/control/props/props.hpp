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

/// Continuous-time predicate (`isct(sys)`). True when `Ts == 0`.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Logical scalar.
/// @see isdt
Value isct(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Discrete-time predicate (`isdt(sys)`).
///
/// True when `Ts > 0` (explicit sample time) or `Ts == -1`
/// (MATLAB's "unspecified discrete").
///
/// @see isct
Value isdt(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// SISO predicate (`issiso(sys)`).
///
/// True for tf / zpk (always SISO in this build) and for ss when
/// `B` has a single column and `C` has a single row.
Value issiso(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Proper rational predicate (`isproper(sys)`).
///
/// - tf:  `length(num) ≤ length(den)`
/// - zpk: `numel(z) ≤ numel(p)`
/// - ss:  always true
Value isproper(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Stability predicate (`isstable(sys)`).
///
/// - continuous: every pole satisfies `real(p) < 0`
/// - discrete:   every pole satisfies `|p| < 1`
///
/// @see pole, isstatic
Value isstable(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// System order (`order(sys)`).
///
/// - tf:  `max(deg(num), deg(den))` = `max(numel-1)`
/// - zpk: `max(numel(z), numel(p))`
/// - ss:  state dimension `rows(A)`
///
/// @return Real scalar Value.
Value order(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Poles of the system (`pole(sys)`).
///
/// Returns a column vector. For ss inputs, computed via the
/// characteristic polynomial of A (Faddeev–LeVerrier + roots).
///
/// @see zero, pzmap, damp
Value pole(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// (Transmission) zeros of the system (`zero(sys)`).
///
/// For tf this is `roots(num)`; for zpk it is the `z` field directly;
/// for SISO ss it goes through @ref ss2tf and `roots`. MIMO ss would
/// require a generalised eigenproblem (QZ) and is not yet supported.
///
/// @throws Error on MIMO ss input.
///
/// @see pole, tzero
Value zero(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Result of @ref damp.
struct DampResult {
    Value wn;     ///< Natural frequencies (column vector, rad/s).
    Value zeta;   ///< Damping ratios     (column vector, dimensionless).
    Value p;      ///< Pole list           (column vector).
};

/// Natural frequencies and damping ratios (`[wn, zeta, p] = damp(sys)`).
///
/// For each pole @f$ s_i @f$:
/// @f$ \omega_{n,i} = |s_i|,\ \zeta_i = -\text{Re}(s_i)/\omega_{n,i} @f$.
/// Discrete-time poles are first converted to the s-plane via
/// @f$ s = \log(z) / T_s @f$.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     @ref DampResult; bind via `auto d = damp(sys);`.
///
/// @see pole
DampResult damp(const Value &sys,
                std::pmr::memory_resource *mr = nullptr);

/// Pole / zero map (`[p, z] = pzmap(sys)`).
///
/// Equivalent to `{pole(sys), zero(sys)}` — convenient when both
/// quantities are needed.
///
/// @return `(poles, zeros)`; bind via `auto [p, z] = pzmap(sys);`.
///
/// @see pole, zero
std::pair<Value, Value>
pzmap(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Static-system predicate (`isstatic(sys)`).
///
/// True when @ref order returns 0 — i.e. the system is a pure gain
/// with no dynamics.
///
/// @see order
Value isstatic(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// Transmission zeros (`tzero(sys)`).
///
/// For SISO inputs equivalent to @ref zero. For MIMO ss inputs a
/// generalised eigenproblem would be required; that path throws.
///
/// @throws Error on MIMO inputs.
///
/// @see zero
Value tzero(const Value &sys, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
