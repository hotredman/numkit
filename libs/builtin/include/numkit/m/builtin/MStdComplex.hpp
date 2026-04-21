// libs/builtin/include/numkit/m/builtin/MStdComplex.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

/// real(x) — real part. For non-complex input returns x unchanged.
MValue real(Allocator &alloc, const MValue &x);

/// imag(x) — imaginary part (as double of the same shape as x).
/// For non-complex input returns scalar 0.
MValue imag(Allocator &alloc, const MValue &x);

/// conj(x) — complex conjugate. For non-complex input returns x unchanged.
MValue conj(Allocator &alloc, const MValue &x);

/// complex(re) — re + 0i, elementwise, same shape as re.
MValue complex(Allocator &alloc, const MValue &re);

/// complex(re, im) — re + im*i elementwise; one side may be scalar and
/// will broadcast. Throws MError on shape mismatch.
MValue complex(Allocator &alloc, const MValue &re, const MValue &im);

/// angle(x) — argument (phase) in radians. For real input uses atan2(0,x)
/// so angle(-1) = pi, angle(0) = 0, etc.
MValue angle(Allocator &alloc, const MValue &x);

} // namespace numkit::m::builtin
