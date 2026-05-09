// libs/signal/include/numkit/signal/filter_design/filter_design.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>

namespace numkit::signal {

/// Butterworth IIR lowpass / highpass filter design.
/// Returns (b, a) — numerator and denominator polynomials.
///
/// @param N     Filter order (integer >= 1).
/// @param Wn    Normalized cutoff frequency in (0, 1) — fraction of Nyquist.
/// @param type  "low" (default) or "high".
/// @throws      Error if Wn is out of (0, 1) or type is unrecognized.
std::tuple<Value, Value>
butter(std::pmr::memory_resource *mr, int N, double Wn, const std::string &type = "low");

/// FIR filter design via windowed-sinc (Hamming window).
/// Returns the impulse response coefficients b (row vector of length N+1).
///
/// @param N     Filter order (integer >= 1). Output length is N+1.
/// @param Wn    Normalized cutoff frequency in (0, 1) — fraction of Nyquist.
/// @param type  "low" (default) or "high".
Value fir1(std::pmr::memory_resource *mr, int N, double Wn, const std::string &type = "low");

/// Least-squares FIR filter design (Type-I linear-phase only).
/// Solves the weighted least-squares problem on a piecewise-linear
/// desired amplitude response specified by frequency-edge / amplitude
/// pairs. Returns the impulse response coefficients b (row vector of
/// length N+1).
///
/// @param N      Filter order (must be even — Type-I only). Length = N+1.
/// @param F      Vector of band-edge frequencies in [0, 1] (Nyquist=1),
///               must have even length and be non-decreasing.
/// @param A      Desired amplitudes at each F point (same length as F).
///               Within each band [F[2k], F[2k+1]] the desired amp is
///               linearly interpolated from A[2k] to A[2k+1].
/// @throws       Error if N is odd, or F/A invalid.
Value firls(std::pmr::memory_resource *mr, int N,
            const double *F, std::size_t Fn,
            const double *A, std::size_t An);

} // namespace numkit::signal
