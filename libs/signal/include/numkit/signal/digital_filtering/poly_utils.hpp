// libs/signal/include/numkit/signal/digital_filtering/poly_utils.hpp
//
// MATLAB Signal Toolbox polynomial utilities.

#pragma once

#include <complex>
#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// @brief Radial scaling of polynomial roots, real-scale form
/// (`y = polyscale(p, scale)`).
///
/// Computes `y[k] = p[k] · scale^k` for `k = 0..N-1` — equivalent to
/// multiplying every root by `scale`. Output is COMPLEX iff `p` is
/// complex.
///
/// @param p      Polynomial coefficient row (descending power order).
/// @param scale  Real scale factor.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scaled polynomial coefficients.
/// @see polystab
Value polyscale(const Value &p, double scale,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Radial scaling of polynomial roots, complex-scale form
/// (`y = polyscale(p, scale)`).
///
/// Same as the real-scale overload but allows a complex `scale`.
/// Output is always COMPLEX.
///
/// @param p      Polynomial coefficient row (descending power order).
/// @param scale  Complex scale factor.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scaled polynomial coefficients, COMPLEX.
/// @see polystab
Value polyscale(const Value &p, std::complex<double> scale,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Stabilise a polynomial (`a = polystab(a)`).
///
/// Reflects any root with `|root| > 1` to its reciprocal-conjugate
/// inside the unit circle, preserving the magnitude response. Returns
/// the resulting polynomial (real if input was real).
///
/// @param a   Polynomial coefficient row.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Stabilised polynomial coefficients.
/// @see polyscale
Value polystab(const Value &a, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
