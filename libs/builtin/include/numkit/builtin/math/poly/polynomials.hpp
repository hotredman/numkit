// libs/builtin/include/numkit/builtin/math/poly/polynomials.hpp
//
// Polynomial-domain builtins.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::builtin {

/// @file
/// @brief Polynomial-domain builtins.
///
/// **Coefficient convention** matches MATLAB: `p(1)` is the leading
/// coefficient, `p(end)` is the constant term. So
/// `p = [1 -3 2] ↔ x² - 3x + 2`.

/// @brief Polynomial roots (`r = roots(p)`).
///
/// Uses the shared Durand-Kerner solver.
/// - Empty / scalar input → `0 × 1` column.
/// - Real polynomial → output column is COMPLEX if any root has a
///   non-trivial imaginary part; otherwise DOUBLE.
/// - Trailing zeros in `p` → corresponding number of roots at 0.
///
/// @param p   Coefficient row.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Column vector of roots (possibly COMPLEX).
/// @see poly, polyval
Value roots(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial derivative (`q = polyder(p)`).
///
/// For `p` of length `n + 1`, the derivative has length `n`.
///
/// @param p   Coefficient row of `p(x)`.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Coefficient row of `p'(x)`.
/// @see polyder(b, a, mr), polyint
Value polyder(const Value &p, std::pmr::memory_resource *mr = nullptr);

/// @brief Derivative of a polynomial ratio
/// (`[num, den] = polyder(b, a)`).
///
/// Returns the coefficients of `d/dx (b(x) / a(x))` as `(numerator,
/// denominator)`. Denominator becomes `a²`; numerator is `a·b' - b·a'`.
///
/// @param b   Numerator coefficients.
/// @param a   Denominator coefficients.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(num, den)` pair.
std::tuple<Value, Value>
polyder(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial antiderivative (`q = polyint(p, k)`).
///
/// Coefficients of `∫ p(x) dx + k`. Output length is `length(p) + 1`.
///
/// @param p   Coefficient row.
/// @param k   Integration constant (default 0).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Antiderivative coefficient row.
/// @see polyder
Value polyint(const Value &p, double k = 0.0, std::pmr::memory_resource *mr = nullptr);

/// @brief Transfer function → zero/pole/gain (`[z, p, k] = tf2zp(b, a)`).
///
/// `H(z) = b(z) / a(z)` → `(zeros, poles, gain)`. `gain = b(1) / a(1)`
/// (leading-coefficient ratio).
///
/// @param b   Numerator coefficients.
/// @param a   Denominator coefficients.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(zeros, poles, gain)` tuple.
/// @see zp2tf
std::tuple<Value, Value, Value>
tf2zp(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// @brief Zero/pole/gain → transfer function (`[b, a] = zp2tf(z, p, k)`).
///
/// `b = k · ∏(x - z)`, `a = ∏(x - p)`. Complex roots must come in
/// conjugate pairs (caller's responsibility — non-conjugate input
/// yields a non-real polynomial silently).
///
/// @param z   Zeros (possibly COMPLEX).
/// @param p   Poles (possibly COMPLEX).
/// @param k   Gain.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(b, a)` coefficient row pair.
/// @see tf2zp
std::tuple<Value, Value>
zp2tf(const Value &z, const Value &p, double k, std::pmr::memory_resource *mr = nullptr);

// ── Curve fitting / evaluation ───────────────────────────────────────

/// @brief Least-squares polynomial fit (`p = polyfit(x, y, n)`).
///
/// Returns a coefficient row in descending power order
/// (`p[0]·x^n + p[1]·x^(n-1) + …`).
///
/// @param x   Sample sites.
/// @param y   Sample values (same length as `x`).
/// @param n   Polynomial degree (need at least `n + 1` points).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Coefficient row of length `n + 1`.
/// @throws Error  Ill-conditioned normal matrix or insufficient data.
/// @see polyval
Value polyfit(const Value &x, const Value &y, int n, std::pmr::memory_resource *mr = nullptr);

/// @brief Horner polynomial evaluation (`y = polyval(p, x)`).
///
/// @param p   Coefficient row.
/// @param x   Evaluation points.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Array of the same shape as `x`.
/// @see polyfit, polyvalm
Value polyval(const Value &p, const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Polynomial with given roots (`p = poly(r)`).
///
/// Returns the coefficient row of the polynomial whose roots are the
/// elements of `r`. Real input → DOUBLE result; complex roots must be
/// passed as a COMPLEX vector (yielding a COMPLEX result).
///
/// @param r   Root vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Coefficient row.
/// @see roots
Value poly(const Value &r, std::pmr::memory_resource *mr = nullptr);

/// @brief Matrix polynomial evaluation (`y = polyvalm(p, A)`).
///
/// `p_0·I + p_1·A + p_2·A² + …` (Horner at the matrix level).
/// `A` must be square.
///
/// @param p   Coefficient row.
/// @param A   Square matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Matrix polynomial value.
/// @throws Error  Non-square `A` (`m:polyvalm:notSquare`).
Value polyvalm(const Value &p, const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Quotient and remainder from `polydiv`.
struct PolyDiv {
    Value q;  ///< Quotient.
    Value r;  ///< Remainder.
};

/// @brief Polynomial long division (`[q, r] = polydiv(b, a)`).
///
/// Returns `q` and `r` such that `b = q · a + r`.
///
/// @param b   Dividend coefficients.
/// @param a   Divisor coefficients.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(q, r)` struct.
PolyDiv polydiv(const Value &b, const Value &a,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Padé approximant of `e^{-T·s}`.
struct PadeCoef {
    Value num;  ///< Numerator coefficients (descending order).
    Value den;  ///< Denominator coefficients (descending order, normalised).
};

/// @brief Padé `(N, N)` approximant coefficients
/// (`[num, den] = padecoef(T, N)`).
///
/// Returns `(num, den)` rows in descending order of `s`, normalised so
/// the leading denominator coefficient is 1 (matches MATLAB's
/// `[num,den] = padecoef(T,N)`).
///
/// @param T   Delay.
/// @param N   Approximation order.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(num, den)` struct.
PadeCoef padecoef(double T, int N,
                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
