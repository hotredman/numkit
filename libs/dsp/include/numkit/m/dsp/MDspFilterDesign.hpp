// libs/dsp/include/numkit/m/dsp/MDspFilterDesign.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

#include <string>
#include <tuple>

namespace numkit::m::dsp {

/// Butterworth IIR lowpass / highpass filter design.
/// Returns (b, a) — numerator and denominator polynomials.
///
/// @param N     Filter order (integer >= 1).
/// @param Wn    Normalized cutoff frequency in (0, 1) — fraction of Nyquist.
/// @param type  "low" (default) or "high".
/// @throws      MError if Wn is out of (0, 1) or type is unrecognized.
std::tuple<MValue, MValue>
butter(Allocator &alloc, int N, double Wn, const std::string &type = "low");

/// FIR filter design via windowed-sinc (Hamming window).
/// Returns the impulse response coefficients b (row vector of length N+1).
///
/// @param N     Filter order (integer >= 1). Output length is N+1.
/// @param Wn    Normalized cutoff frequency in (0, 1) — fraction of Nyquist.
/// @param type  "low" (default) or "high".
MValue fir1(Allocator &alloc, int N, double Wn, const std::string &type = "low");

/// Frequency response H(e^{j omega}) of an IIR/FIR filter: H = b(e^{-jw}) / a(e^{-jw}).
/// Returns (H, W) — complex response and the grid of frequencies w in [0, pi].
///
/// @param b     Numerator polynomial.
/// @param a     Denominator polynomial.
/// @param npts  Number of frequency points (default 512).
std::tuple<MValue, MValue>
freqz(Allocator &alloc, const MValue &b, const MValue &a, size_t npts = 512);

} // namespace numkit::m::dsp
