// libs/signal/include/numkit/signal/windows/windows.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Hamming window, column vector of length N.
Value hamming(Allocator &alloc, size_t N);

/// Hann (also known as Hanning) window, column vector of length N.
Value hann(Allocator &alloc, size_t N);

/// Blackman window, column vector of length N.
Value blackman(Allocator &alloc, size_t N);

/// Kaiser window, column vector of length N with shape parameter beta.
Value kaiser(Allocator &alloc, size_t N, double beta);

/// Rectangular window, column vector of length N — all ones.
Value rectwin(Allocator &alloc, size_t N);

/// Bartlett (triangular) window, column vector of length N.
Value bartlett(Allocator &alloc, size_t N);

} // namespace numkit::signal
