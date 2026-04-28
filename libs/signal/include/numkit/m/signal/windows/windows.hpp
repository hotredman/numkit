// libs/dsp/include/numkit/m/signal/windows/windows.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::signal {

/// Hamming window, column vector of length N.
MValue hamming(Allocator &alloc, size_t N);

/// Hann (also known as Hanning) window, column vector of length N.
MValue hann(Allocator &alloc, size_t N);

/// Blackman window, column vector of length N.
MValue blackman(Allocator &alloc, size_t N);

/// Kaiser window, column vector of length N with shape parameter beta.
MValue kaiser(Allocator &alloc, size_t N, double beta);

/// Rectangular window, column vector of length N — all ones.
MValue rectwin(Allocator &alloc, size_t N);

/// Bartlett (triangular) window, column vector of length N.
MValue bartlett(Allocator &alloc, size_t N);

} // namespace numkit::m::signal
