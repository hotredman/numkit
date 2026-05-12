// libs/builtin/include/numkit/builtin/math/complex/complex.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// real(x) — real part. For non-complex input returns x unchanged.
Value real(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// imag(x) — imaginary part (as double of the same shape as x).
/// For non-complex input returns scalar 0.
Value imag(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// conj(x) — complex conjugate. For non-complex input returns x unchanged.
Value conj(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// complex(re) — re + 0i, elementwise, same shape as re.
Value complex(const Value &re, std::pmr::memory_resource *mr = nullptr);

/// complex(re, im) — re + im*i elementwise; one side may be scalar and
/// will broadcast. Throws Error on shape mismatch.
Value complex(const Value &re, const Value &im, std::pmr::memory_resource *mr = nullptr);

/// angle(x) — argument (phase) in radians. For real input uses atan2(0,x)
/// so angle(-1) = pi, angle(0) = 0, etc.
Value angle(const Value &x, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::builtin
