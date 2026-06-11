// toolboxes/builtin/include/numkit/builtin/math/complex/complex.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::math {

/// @brief Real part (`y = real(x)`).
///
/// For non-complex input returns `x` unchanged. Elementwise.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Real part, same shape as `x`.
/// @see imag, conj
Value real(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Imaginary part (`y = imag(x)`).
///
/// For non-complex input returns scalar 0.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Imaginary part as DOUBLE, same shape as `x`
///            (or scalar 0 for non-complex `x`).
/// @see real, conj
Value imag(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Complex conjugate (`y = conj(x)`).
///
/// For non-complex input returns `x` unchanged.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Conjugate, same shape as `x`.
/// @see real, imag
Value conj(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Single-arg complex constructor (`y = complex(re)`).
///
/// `y = re + 0i` elementwise. Useful for forcing a real array into the
/// COMPLEX type.
///
/// @param re  Real part (any numeric type).
/// @param mr  Memory resource (nullptr → process default).
/// @return    COMPLEX array, same shape as `re`.
/// @see complex(re, im)
Value complex(const Value &re, std::pmr::memory_resource *mr = nullptr);

/// @brief Two-arg complex constructor (`y = complex(re, im)`).
///
/// `y = re + im · i` elementwise. One side may be scalar and will
/// broadcast. Throws on shape mismatch.
///
/// @param re  Real part.
/// @param im  Imaginary part.
/// @param mr  Memory resource (nullptr → process default).
/// @return    COMPLEX array, broadcast shape.
/// @throws Error  Shape mismatch (`m:complex:badShape`).
Value complex(const Value &re, const Value &im, std::pmr::memory_resource *mr = nullptr);

/// @brief Argument / phase angle (`y = angle(x)`).
///
/// Returns the argument in radians. For real input uses `atan2(0, x)`
/// so `angle(-1) = π`, `angle(0) = 0`, etc.
///
/// @param x   Input array.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Phase angles in `(-π, π]`.
Value angle(const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::math
