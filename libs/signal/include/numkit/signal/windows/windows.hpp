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

/// Triangular window — w(n) = 1 - |2n - (N-1)| / N (odd N) or
/// 1 - |2n - (N-1)| / (N-1) (even N). Endpoints are NOT zero (unlike
/// `bartlett`, which is `triang` with the endpoints forced to zero).
Value triang(std::pmr::memory_resource *mr, size_t N);

/// Tukey (tapered cosine) window with cosine-fraction r in [0, 1].
/// r=0 → rectangular, r=1 → Hann. Default r = 0.5.
Value tukeywin(std::pmr::memory_resource *mr, size_t N, double r = 0.5);

/// Flat-top window — 5-term cosine sum, optimised for amplitude accuracy.
Value flattopwin(std::pmr::memory_resource *mr, size_t N);

/// Gaussian window — w(n) = exp(-0.5·(α·(n - (N-1)/2)/((N-1)/2))²).
/// Default alpha = 2.5 (matches MATLAB).
Value gausswin(std::pmr::memory_resource *mr, size_t N, double alpha = 2.5);

/// Dolph-Chebyshev window — equiripple sidelobes at `at` dB attenuation.
/// `at` is positive (e.g. 100 → -100 dB sidelobes). Default at = 100.
Value chebwin(std::pmr::memory_resource *mr, size_t N, double at = 100.0);

/// Parzen (de la Vallée Poussin) window — piecewise cubic.
Value parzenwin(std::pmr::memory_resource *mr, size_t N);

/// Nuttall (4-term symmetric, continuous-derivative) window.
Value nuttallwin(std::pmr::memory_resource *mr, size_t N);

/// Taylor window — sidelobe-controlled, used in radar / antenna design.
/// nbar = number of nearly-equal-height sidelobes (default 4).
/// sll = peak sidelobe level in dB (default -30, must be < 0).
Value taylorwin(std::pmr::memory_resource *mr, size_t N, int nbar = 4,
                double sll = -30.0);

/// Blackman-Harris (4-term, minimum-sidelobe) window.
Value blackmanharris(std::pmr::memory_resource *mr, size_t N);

/// Bohman window — convolution of two half-cosine pulses.
Value bohmanwin(std::pmr::memory_resource *mr, size_t N);

/// Modified Bartlett-Hann window.
Value barthannwin(std::pmr::memory_resource *mr, size_t N);

} // namespace numkit::signal
