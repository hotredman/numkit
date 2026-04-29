// libs/builtin/include/numkit/builtin/math/elementary/complex.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// real(x) — real part. For non-complex input returns x unchanged.
Value real(std::pmr::memory_resource *mr, const Value &x);

/// imag(x) — imaginary part (as double of the same shape as x).
/// For non-complex input returns scalar 0.
Value imag(std::pmr::memory_resource *mr, const Value &x);

/// conj(x) — complex conjugate. For non-complex input returns x unchanged.
Value conj(std::pmr::memory_resource *mr, const Value &x);

/// complex(re) — re + 0i, elementwise, same shape as re.
Value complex(std::pmr::memory_resource *mr, const Value &re);

/// complex(re, im) — re + im*i elementwise; one side may be scalar and
/// will broadcast. Throws Error on shape mismatch.
Value complex(std::pmr::memory_resource *mr, const Value &re, const Value &im);

/// angle(x) — argument (phase) in radians. For real input uses atan2(0,x)
/// so angle(-1) = pi, angle(0) = 0, etc.
Value angle(std::pmr::memory_resource *mr, const Value &x);

} // namespace numkit::builtin
