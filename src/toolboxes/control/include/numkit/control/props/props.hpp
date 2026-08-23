// toolboxes/control/include/numkit/control/props/props.hpp
//
// Predicates and analytic properties of an LTI struct value (tf, zpk,
// or ss as built by toolboxes/control/lti).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <utility>

namespace numkit::control {

/// @addtogroup group_control
/// @{


/// @file
/// @ingroup group_control
/// @brief LTI-system predicates and analytic properties.
///
/// Every function in this header operates on the `kind`-tagged LTI
/// struct produced by the cycle-31 constructors (`tf`, `zpk`, `ss`).
/// `mr = nullptr` selects the process default memory resource.

/// @brief Continuous-time predicate (`isct(sys)`).
///
/// True when `Ts == 0`.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     LOGICAL scalar.
/// @see isdt
Value isct(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// @brief Discrete-time predicate (`isdt(sys)`).
///
/// True when `Ts > 0` (explicit sample time) or `Ts == -1`
/// ("unspecified discrete").
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     LOGICAL scalar.
/// @see isct
Value isdt(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// @brief SISO predicate (`issiso(sys)`).
///
/// True for `tf` / `zpk` (always SISO in this build) and for `ss` when
/// `B` has a single column and `C` has a single row.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     LOGICAL scalar.
Value issiso(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// @brief Proper rational predicate (`isproper(sys)`).
///
/// - `tf`:  `length(num) <= length(den)`
/// - `zpk`: `numel(z) <= numel(p)`
/// - `ss`:  always true
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     LOGICAL scalar.
Value isproper(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// @brief Stability predicate (`isstable(sys)`).
///
/// - continuous: every pole satisfies `real(p) < 0`.
/// - discrete:   every pole satisfies `|p| < 1`.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     LOGICAL scalar.
/// @see pole, isstatic
Value isstable(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// @brief System order (`order(sys)`).
///
/// - `tf`:  `max(deg(num), deg(den))` = `max(numel - 1)`
/// - `zpk`: `max(numel(z), numel(p))`
/// - `ss`:  state dimension `rows(A)`
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     DOUBLE scalar.
/// @see isstatic
Value order(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// @brief Poles of the system (`pole(sys)`).
///
/// Column vector. For `ss` inputs, computed via the characteristic
/// polynomial of `A` (Faddeev-LeVerrier + roots).
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Column vector of poles (possibly COMPLEX).
/// @see zero, pzmap, damp
Value pole(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// @brief (Transmission) zeros of the system (`zero(sys)`).
///
/// For `tf` this is `roots(num)`; for `zpk` it is the `z` field directly;
/// for SISO `ss` it goes through @ref ss2tf and `roots`. MIMO `ss` would
/// require a generalised eigenproblem (QZ) and is not yet supported.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Column vector of zeros.
/// @throws Error  MIMO `ss` input.
/// @see pole, tzero
Value zero(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// @brief Result of @ref damp.
struct DampResult {
    Value wn;     ///< Natural frequencies (column vector, rad/s).
    Value zeta;   ///< Damping ratios     (column vector, dimensionless).
    Value p;      ///< Pole list           (column vector).
};

/// @brief Natural frequencies and damping ratios
/// (`[wn, zeta, p] = damp(sys)`).
///
/// For each pole @f$ s_i @f$:
/// @f$ \omega_{n,i} = |s_i|,\ \zeta_i = -\text{Re}(s_i)/\omega_{n,i} @f$.
/// Discrete-time poles are first converted to the s-plane via
/// @f$ s = \log(z) / T_s @f$.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     @ref DampResult; bind via `auto d = damp(sys);`.
/// @see pole
DampResult damp(const Value &sys,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Pole / zero map (`[p, z] = pzmap(sys)`).
///
/// Equivalent to `{pole(sys), zero(sys)}` — convenient when both
/// quantities are needed.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     `(poles, zeros)` pair; bind via `auto [p, z] = pzmap(sys);`.
/// @see pole, zero
std::pair<Value, Value>
pzmap(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// @brief Static-system predicate (`isstatic(sys)`).
///
/// True when @ref order returns 0 — i.e. the system is a pure gain
/// with no dynamics.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     LOGICAL scalar.
/// @see order
Value isstatic(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// @brief Transmission zeros (`tzero(sys)`).
///
/// For SISO inputs equivalent to @ref zero. For MIMO `ss` inputs a
/// generalised eigenproblem would be required; that path throws.
///
/// @param sys  LTI struct.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Column vector of transmission zeros.
/// @throws Error  MIMO `ss` input.
/// @see zero
Value tzero(const Value &sys, std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::control
