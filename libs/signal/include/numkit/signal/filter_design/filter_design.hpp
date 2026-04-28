// libs/signal/include/numkit/signal/filter_design/filter_design.hpp
#pragma once

#include <numkit/core/allocator.hpp>
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
butter(Allocator &alloc, int N, double Wn, const std::string &type = "low");

/// FIR filter design via windowed-sinc (Hamming window).
/// Returns the impulse response coefficients b (row vector of length N+1).
///
/// @param N     Filter order (integer >= 1). Output length is N+1.
/// @param Wn    Normalized cutoff frequency in (0, 1) — fraction of Nyquist.
/// @param type  "low" (default) or "high".
Value fir1(Allocator &alloc, int N, double Wn, const std::string &type = "low");

} // namespace numkit::signal
