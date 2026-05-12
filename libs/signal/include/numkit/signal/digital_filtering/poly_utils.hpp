// libs/signal/include/numkit/signal/digital_filtering/poly_utils.hpp
//
// MATLAB Signal Toolbox polynomial utilities.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// @brief Radial scaling of polynomial roots (`y = polyscale(p, scale)`).
///
/// Computes `y[k] = p[k] · scale^k` for `k = 0..N-1`. Equivalent to
/// multiplying every root of the polynomial by `scale`. Real or complex
/// inputs supported.
///
/// @param p      Polynomial coefficient row (descending power order).
/// @param scale  Scalar (or complex) scale factor.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scaled polynomial coefficients.
/// @see polystab
Value polyscale(const Value &p, const Value &scale,
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
