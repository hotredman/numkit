// libs/signal/include/numkit/signal/digital_filtering/poly_utils.hpp
//
// Signal-processing polynomial utilities.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// @brief Radial scaling of polynomial roots (`y = polyscale(p, scale)`).
///
/// Computes `y[k] = p[k] · scale^k` for `k = 0..N-1`, equivalent to
/// multiplying every root of the polynomial by `scale` (the z-transform
/// scaling property `A(z) ↦ A(z/scale)`). Real or complex inputs
/// supported.
///
/// @param p      Polynomial coefficients. A vector is one polynomial of
///               length `N`; an `M×N` matrix is `M` polynomials, one per
///               row, each scaled independently.
/// @param scale  Scaling factor — a scalar, or a length-`N` vector whose
///               element `k` is raised to the power `k`.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scaled polynomial coefficients (row vector for a vector
///               input, `M×N` matrix for a matrix input).
/// @see polystab
Value polyscale(const Value &p, const Value &scale,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Stabilise a polynomial (`b = polystab(a)`).
///
/// Reflects any root with `|root| > 1` to its conjugate reciprocal
/// `1/conj(root)` inside the unit circle, yielding the minimum-phase
/// polynomial. The magnitude-response *shape* is preserved — the
/// reflection scales `|B(e^jω)|` by a constant gain relative to
/// `|A(e^jω)|`. Returns a row vector (real if the input was real).
///
/// @param a   Polynomial coefficients, a vector (row or column).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Stabilised polynomial coefficients (row vector).
/// @note Complex-coefficient input is not supported — numkit's `roots`
///       handles real polynomials only.
/// @see polyscale
Value polystab(const Value &a, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
