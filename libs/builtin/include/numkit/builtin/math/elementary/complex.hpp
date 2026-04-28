// libs/builtin/include/numkit/builtin/math/elementary/complex.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

/// real(x) — real part. For non-complex input returns x unchanged.
Value real(Allocator &alloc, const Value &x);

/// imag(x) — imaginary part (as double of the same shape as x).
/// For non-complex input returns scalar 0.
Value imag(Allocator &alloc, const Value &x);

/// conj(x) — complex conjugate. For non-complex input returns x unchanged.
Value conj(Allocator &alloc, const Value &x);

/// complex(re) — re + 0i, elementwise, same shape as re.
Value complex(Allocator &alloc, const Value &re);

/// complex(re, im) — re + im*i elementwise; one side may be scalar and
/// will broadcast. Throws Error on shape mismatch.
Value complex(Allocator &alloc, const Value &re, const Value &im);

/// angle(x) — argument (phase) in radians. For real input uses atan2(0,x)
/// so angle(-1) = pi, angle(0) = 0, etc.
Value angle(Allocator &alloc, const Value &x);

} // namespace numkit::builtin
