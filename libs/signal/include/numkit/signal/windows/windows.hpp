// libs/signal/include/numkit/signal/windows/windows.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// Hamming window, column vector of length N.
Value hamming(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Hann (also known as Hanning) window, column vector of length N.
Value hann(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Blackman window, column vector of length N.
Value blackman(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Kaiser window, column vector of length N with shape parameter beta.
Value kaiser(size_t N, double beta, std::pmr::memory_resource *mr = nullptr);

/// Rectangular window, column vector of length N — all ones.
Value rectwin(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Bartlett (triangular) window, column vector of length N.
Value bartlett(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Triangular window — w(n) = 1 - |2n - (N-1)| / N (odd N) or
/// 1 - |2n - (N-1)| / (N-1) (even N). Endpoints are NOT zero (unlike
/// `bartlett`, which is `triang` with the endpoints forced to zero).
Value triang(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Tukey (tapered cosine) window with cosine-fraction r in [0, 1].
/// r=0 → rectangular, r=1 → Hann. Default r = 0.5.
Value tukeywin(size_t N, double r = 0.5,
               std::pmr::memory_resource *mr = nullptr);

/// Flat-top window — 5-term cosine sum, optimised for amplitude accuracy.
Value flattopwin(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Gaussian window — w(n) = exp(-0.5·(α·(n - (N-1)/2)/((N-1)/2))²).
/// Default alpha = 2.5 (matches MATLAB).
Value gausswin(size_t N, double alpha = 2.5,
               std::pmr::memory_resource *mr = nullptr);

/// Dolph-Chebyshev window — equiripple sidelobes at `at` dB attenuation.
/// `at` is positive (e.g. 100 → -100 dB sidelobes). Default at = 100.
Value chebwin(size_t N, double at = 100.0,
              std::pmr::memory_resource *mr = nullptr);

/// Parzen (de la Vallée Poussin) window — piecewise cubic.
Value parzenwin(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Nuttall (4-term symmetric, continuous-derivative) window.
Value nuttallwin(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Taylor window — sidelobe-controlled, used in radar / antenna design.
/// nbar = number of nearly-equal-height sidelobes (default 4).
/// sll = peak sidelobe level in dB (default -30, must be < 0).
Value taylorwin(size_t N, int nbar = 4, double sll = -30.0,
                std::pmr::memory_resource *mr = nullptr);

/// Blackman-Harris (4-term, minimum-sidelobe) window.
Value blackmanharris(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Bohman window — convolution of two half-cosine pulses.
Value bohmanwin(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Modified Bartlett-Hann window.
Value barthannwin(size_t N, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
