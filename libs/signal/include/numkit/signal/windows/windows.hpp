// libs/signal/include/numkit/signal/windows/windows.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Hamming window, column vector of length N.
Value hamming(std::pmr::memory_resource *mr, size_t N);

/// Hann (also known as Hanning) window, column vector of length N.
Value hann(std::pmr::memory_resource *mr, size_t N);

/// Blackman window, column vector of length N.
Value blackman(std::pmr::memory_resource *mr, size_t N);

/// Kaiser window, column vector of length N with shape parameter beta.
Value kaiser(std::pmr::memory_resource *mr, size_t N, double beta);

/// Rectangular window, column vector of length N — all ones.
Value rectwin(std::pmr::memory_resource *mr, size_t N);

/// Bartlett (triangular) window, column vector of length N.
Value bartlett(std::pmr::memory_resource *mr, size_t N);

} // namespace numkit::signal
