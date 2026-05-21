// libs/signal/include/numkit/signal/filter_analysis/predicates.hpp
//
// Boolean classifiers for digital filters: isfir / isallpass / isstable /
// islinphase / isminphase / ismaxphase. All take (b) or (b, a) and
// return a logical scalar. Tolerances follow MATLAB's defaults.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Detects whether a digital filter is FIR.
///
/// A filter is FIR when its denominator is effectively `[1]` —
/// either a literal scalar `1`, or a vector with only `a[0]` non-zero
/// (within tolerance 1e-12).
///
/// @param b   Numerator polynomial.
/// @return    `true` if FIR. The single-arg form assumes `a == [1]`.
bool isfir(const Value &b);

/// @brief Detects FIR with explicit denominator.
///
/// Same FIR criterion as @ref isfir(const Value &): denominator
/// must be effectively `[1]` within tolerance.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @return    `true` if FIR.
bool isfir(const Value &b, const Value &a);

/// Detects whether a digital filter is BIBO-stable.
///
/// Returns `true` iff every pole of `A(z)` is strictly inside the unit
/// circle (`|p| < 1 - tol`, tol = 1e-12).
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param mr  Memory resource for the internal root-finder (nullptr → default).
/// @return    `true` if stable.
///
/// @see isminphase, ismaxphase
bool isstable(const Value &                b,
              const Value &                a,
              std::pmr::memory_resource *  mr = nullptr);

/// Detects whether a digital filter is minimum-phase.
///
/// True iff every zero of `B(z)` lies inside the unit circle AND the
/// filter is stable.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `true` if minimum-phase.
///
/// @see isstable, ismaxphase
bool isminphase(const Value &                b,
                const Value &                a,
                std::pmr::memory_resource *  mr = nullptr);

/// Detects whether a digital filter is maximum-phase.
///
/// True iff every zero of `B(z)` lies strictly outside the unit circle
/// (`|z| > 1 + tol`).
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `true` if maximum-phase.
///
/// @see isminphase
bool ismaxphase(const Value &                b,
                const Value &                a,
                std::pmr::memory_resource *  mr = nullptr);

/// Detects whether a digital filter has linear phase.
///
/// For FIR filters this is equivalent to `b` being symmetric
/// (`b == flip(b)`) or antisymmetric (`b == -flip(b)`).
/// IIR with non-trivial denominator → returns `false` (linear phase is
/// impossible for stable rational IIR).
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @return    `true` if linear-phase.
///
/// @see firtype, isfir
bool islinphase(const Value &b, const Value &a);

/// Detects whether a digital filter is all-pass.
///
/// All-pass means `|H(e^{jω})| = const` for all ω. Equivalent algebraic
/// test (within tolerance): `a == flip(b)`.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @return    `true` if all-pass.
bool isallpass(const Value &b, const Value &a);

/// Filter order.
///
/// Returns:
///   * FIR (a omitted or `a == [1]`): `length(b_trimmed) - 1`.
///   * IIR: `max(length(b_trimmed), length(a_trimmed)) - 1`.
///
/// Trailing zeros of `b` and `a` are trimmed before counting.
///
/// @param b   Numerator polynomial.
/// @return    Integer filter order.
int filtord(const Value &b);

/// @brief Filter order with explicit denominator (`n = filtord(b, a)`).
///
/// IIR formula: `max(length(b_trimmed), length(a_trimmed)) - 1`.
/// Trailing zeros of `b` and `a` are trimmed before counting.
///
/// @param b   Numerator polynomial.
/// @param a   Denominator polynomial.
/// @return    Integer filter order.
int filtord(const Value &b, const Value &a);

/// FIR filter type classification (1–4).
///
///   * Type 1: even order (odd length), b symmetric.
///   * Type 2: odd  order (even length), b symmetric.
///   * Type 3: even order (odd length), b antisymmetric.
///   * Type 4: odd  order (even length), b antisymmetric.
///
/// @param b   Numerator polynomial of a linear-phase FIR.
/// @return    Type number in `{1, 2, 3, 4}`.
/// @throws    numkit::Error  if `b` is neither symmetric nor antisymmetric
///                           within tolerance.
///
/// @see islinphase
int firtype(const Value &b);

/// L_p norm of a digital filter's frequency response.
///
/// For \f$ p = 2 \f$ (default, energy norm):
/// \f[
///   \|H\|_2 = \sqrt{\frac{1}{\pi} \int_0^{\pi} |H(e^{j\omega})|^2 \, d\omega}
/// \f]
///
/// For \f$ p = \infty \f$ (peak gain):
/// \f[
///   \|H\|_\infty = \max_{\omega \in [0, \pi]} |H(e^{j\omega})|
/// \f]
///
/// @param b      Numerator polynomial. Must be non-empty.
/// @param a      Denominator polynomial. Must be non-empty and `a[0] != 0`.
/// @param pnorm  Either `2.0` (default) or `std::numeric_limits<double>::infinity()`.
/// @param mr     Memory resource for the internal freqz grid (nullptr → default).
/// @return       Non-negative L_p norm. `+inf` when the filter is unstable
///               and `pnorm == inf`.
/// @throws       numkit::Error  if `b` / `a` empty, `a[0] == 0`, or `pnorm`
///                              is neither 2 nor +inf.
///
/// @note Internally evaluates `freqz` on a uniform 8192-point grid on
///       \f$ [0, \pi] \f$ (the default `NFFT`).
///
/// @code
/// auto [b, a] = butter(4, 0.3);
/// double e = filternorm(b, a);                                    // L_2
/// double p = filternorm(b, a, std::numeric_limits<double>::infinity()); // L_inf
/// @endcode
///
/// @see freqz, isstable
double filternorm(const Value &                b,
                  const Value &                a,
                  double                       pnorm = 2.0,
                  std::pmr::memory_resource *  mr    = nullptr);

} // namespace numkit::signal
